/*
 * EcoSync SCADA — Módulo A: Telemetria de Volumetria
 * Hardware : ESP8266 NodeMCU ideaspark HW-364A (OLED 0.96" integrado)
 * Sensor   : HC-SR04
 *
 * Pinos usados:
 *   OLED SDA → D5 (GPIO14)   OLED SCL → D6 (GPIO12)
 *   HC-SR04 TRIG → D2 (GPIO4)
 *   HC-SR04 ECHO → D1 (GPIO5)  ← usar divisor de tensão!
 *   Botão A (Navegar)  → D7 (GPIO13)
 *   Botão B (Confirmar)→ D3 (GPIO0)  ← não segurar ao ligar!
 *
 * Divisor de tensão no pino ECHO (HC-SR04 é 5V, ESP é 3.3V):
 *   ECHO ──[1kΩ]──┬── GPIO5
 *                [2kΩ]
 *                 │
 *                GND
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <scMQTTLib.h>

// ================================================================
// CONFIGURAÇÃO — EDITE AQUI
// ================================================================
const char* WIFI_SSID     = "SEU_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA";

// Credenciais do broker MQTT (apisilas.ddns.net) — NÃO COMMITAR este arquivo
// com as credenciais reais preenchidas, o repositório é público no GitHub!
const char* MQTT_USUARIO = "SEU_USUARIO_API";
const char* MQTT_SENHA   = "SUA_SENHA_API";

// ================================================================
// PINOS
// ================================================================
#define OLED_SDA    14  // D5
#define OLED_SCL    12  // D6
#define TRIG_PIN     4  // D2
#define ECHO_PIN     5  // D1 (com divisor de tensão)
#define BTN_A_PIN   13  // D7 — Navegar
#define BTN_B_PIN    0  // D3 — Confirmar

// ================================================================
// DISPLAY
// ================================================================
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================================================================
// MQTT
// ================================================================
scMQTTLib mqtt(MQTT_USUARIO, MQTT_SENHA);

// ================================================================
// EEPROM
// ================================================================
#define EEPROM_SIZE      16
#define ADDR_DIST_FULL    0
#define ADDR_DIST_EMPTY   4
#define ADDR_MAX_LITERS   8
#define ADDR_VALID       12

// ================================================================
// ULTRASSÔNICO — não bloqueante via interrupção
// ================================================================
volatile unsigned long echoStart    = 0;
volatile unsigned long echoDuration = 0;
volatile bool          echoReady    = false;

void ICACHE_RAM_ATTR echoISR() {
  if (digitalRead(ECHO_PIN) == HIGH) {
    echoStart = micros();
  } else if (echoStart > 0) {
    echoDuration = micros() - echoStart;
    echoReady    = true;
    echoStart    = 0;
  }
}

void sendTrigger() {
  echoReady = false;
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
}

// Filtro de mediana — descarta leituras isoladas corrompidas sem
// bloquear o loop
#define DIST_SAMPLES 3
float distSamples[DIST_SAMPLES] = { 0, 0, 0 };
int   distSampleIndex           = 0;
int   distSampleCount           = 0;

float medianDist() {
  float sorted[DIST_SAMPLES];
  memcpy(sorted, distSamples, distSampleCount * sizeof(float));
  for (int i = 1; i < distSampleCount; i++) {
    float key = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  return sorted[distSampleCount / 2];
}

// ================================================================
// CALIBRAÇÃO
// ================================================================
struct Calibration {
  float distFull;
  float distEmpty;
  float maxLiters;
  bool  valid;
} cal = { 5.0, 25.0, 2.0, false };

void loadCalibration() {
  if (EEPROM.read(ADDR_VALID) != 0xAB) return;
  EEPROM.get(ADDR_DIST_FULL,  cal.distFull);
  EEPROM.get(ADDR_DIST_EMPTY, cal.distEmpty);
  EEPROM.get(ADDR_MAX_LITERS, cal.maxLiters);
  cal.valid = true;
}

void saveCalibration() {
  EEPROM.put(ADDR_DIST_FULL,  cal.distFull);
  EEPROM.put(ADDR_DIST_EMPTY, cal.distEmpty);
  EEPROM.put(ADDR_MAX_LITERS, cal.maxLiters);
  EEPROM.write(ADDR_VALID, 0xAB);
  EEPROM.commit();
  cal.valid = true;
}

// ================================================================
// ESTADO DO SISTEMA
// ================================================================
float currentDist    = 0.0;
float currentPercent = 0.0;
float currentLiters  = 0.0;

float calcPercent(float dist) {
  if (!cal.valid)             return 0.0;
  if (dist <= cal.distFull)   return 100.0;
  if (dist >= cal.distEmpty)  return 0.0;
  return 100.0 * (cal.distEmpty - dist) / (cal.distEmpty - cal.distFull);
}

const char* getStatus(float pct) {
  if (pct > 60.0) return "NORMAL";
  if (pct > 25.0) return "BAIXO";
  return "CRITICO";
}

// ================================================================
// ESTADOS DA INTERFACE
// ================================================================
enum AppState {
  S_MAIN,
  S_MENU,
  S_CAL_FULL,
  S_CAL_EMPTY,
  S_CONFIG_LITERS
};

AppState appState  = S_MAIN;
int      menuIndex = 0;

const char* MENU_ITEMS[] = {
  "Ver volume",
  "Calibrar CHEIO",
  "Calibrar VAZIO",
  "Config. Litros"
};
const int MENU_COUNT = 4;

float configLitersTemp = 0.0;

// ================================================================
// BOTÕES
// ================================================================
struct Button {
  uint8_t       pin;
  bool          lastState;
  bool          pressed;
  unsigned long lastDebounce;
};

Button btnA = { BTN_A_PIN, false, false, 0 };
Button btnB = { BTN_B_PIN, false, false, 0 };

void updateButtons() {
  unsigned long now = millis();

  bool aRaw = (digitalRead(BTN_A_PIN) == LOW);
  if (aRaw && !btnA.lastState && (now - btnA.lastDebounce > 150)) {
    btnA.pressed      = true;
    btnA.lastDebounce = now;
  }
  btnA.lastState = aRaw;

  bool bRaw = (digitalRead(BTN_B_PIN) == LOW);
  if (bRaw && !btnB.lastState && (now - btnB.lastDebounce > 150)) {
    btnB.pressed      = true;
    btnB.lastDebounce = now;
  }
  btnB.lastState = bRaw;
}

// ================================================================
// TEMPORIZAÇÃO
// ================================================================
unsigned long lastTrigger     = 0;
unsigned long lastMqttPub     = 0;
unsigned long lastDisplayDraw = 0;

// ================================================================
// MQTT
// ================================================================
void publishData() {
  if (WiFi.status() != WL_CONNECTED) return;

  JsonDocument doc;
  doc["nivel_pct"]    = currentPercent;
  doc["volume_l"]     = currentLiters;
  doc["distancia_cm"] = currentDist;
  doc["status"]       = getStatus(currentPercent);
  doc["max_litros"]   = cal.maxLiters;

  String payload;
  serializeJson(doc, payload);

  Serial.println(payload);
  mqtt.enviarJSON(payload);
}

// ================================================================
// TELAS DO OLED
// ================================================================

void drawMainScreen() {
  display.clearDisplay();

  // Barra de progresso
  display.drawRect(2, 1, 124, 10, SSD1306_WHITE);
  int barFill = (int)(currentPercent * 120.0 / 100.0);
  if (barFill > 0) display.fillRect(2, 1, barFill, 10, SSD1306_WHITE);

  // Porcentagem em destaque
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 14);
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)currentPercent);
  display.print(pctBuf);

  // Litros
  display.setTextSize(1);
  display.setCursor(4, 44);
  char litBuf[24];
  snprintf(litBuf, sizeof(litBuf), "%.1fL / %.1fL", currentLiters, cal.maxLiters);
  display.print(litBuf);

  // Status
  display.setCursor(4, 54);
  display.print(getStatus(currentPercent));

  // Aviso calibração
  if (!cal.valid) {
    display.setCursor(70, 44);
    display.print("!Calibrar");
  }

  // WiFi + hint
  display.setCursor(70, 54);
  display.print(WiFi.status() == WL_CONNECTED ? "WiFi OK" : "Sem WiFi");

  display.display();
}

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(46, 0);
  display.print("MENU");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 14 + i * 13;
    if (i == menuIndex) {
      display.fillRect(0, y - 1, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(4, y);
    display.print(MENU_ITEMS[i]);
  }

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 57);
  display.print("[A]Nav [B]OK");

  display.display();
}

void drawCalConfirm(bool isFull) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(4, 0);
  display.print(isFull ? "Calibrar CHEIO" : "Calibrar VAZIO");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(4, 16);
  char distBuf[24];
  snprintf(distBuf, sizeof(distBuf), "Dist: %.1f cm", currentDist);
  display.print(distBuf);

  display.setCursor(4, 30);
  display.print(isFull ? "Reserv. CHEIO" : "Reserv. VAZIO");
  display.setCursor(4, 42);
  display.print("Aponte e salve.");

  display.setCursor(0, 55);
  display.print("[A]Voltar  [B]Salvar");

  display.display();
}

void drawConfigLiters() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(4, 0);
  display.print("Vol. maximo (L)");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setTextSize(3);
  display.setCursor(20, 22);
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", configLitersTemp);
  display.print(buf);

  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print("[A]+0.2L  [B]Salvar");

  display.display();
}

// ================================================================
// LÓGICA DO MENU
// ================================================================
void handleMenuSelect() {
  switch (menuIndex) {
    case 0: appState = S_MAIN; break;
    case 1: appState = S_CAL_FULL; break;
    case 2: appState = S_CAL_EMPTY; break;
    case 3:
      configLitersTemp = cal.maxLiters;
      appState = S_CONFIG_LITERS;
      break;
  }
}

// ================================================================
// SETUP
// ================================================================
void setup() {
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  loadCalibration();

  pinMode(TRIG_PIN,  OUTPUT);
  pinMode(ECHO_PIN,  INPUT);
  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
  digitalWrite(TRIG_PIN, LOW);

  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoISR, CHANGE);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(18, 20);
  display.print("EcoSync SCADA");
  display.setCursor(14, 35);
  display.print("Volumetria  v1.0");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long wStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wStart < 8000) {
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    ArduinoOTA.setHostname("ecosync-volumetria");
    ArduinoOTA.begin();
  }

  mqtt.iniciar();
}

// ================================================================
// LOOP PRINCIPAL
// ================================================================
void loop() {
  ArduinoOTA.handle();
  mqtt.manterConexao();

  if (mqtt.temComando()) {
    String cmd = mqtt.obterComando();
    Serial.print("Comando recebido: ");
    Serial.println(cmd);

    if (cmd == "calibrar_cheio") {
      cal.distFull = currentDist;
      saveCalibration();
    } else if (cmd == "calibrar_vazio") {
      cal.distEmpty = currentDist;
      saveCalibration();
    } else if (cmd == "reset") {
      cal.valid = false;
      EEPROM.write(ADDR_VALID, 0x00);
      EEPROM.commit();
    }
  }

  updateButtons();

  unsigned long now = millis();

  if (now - lastTrigger >= 500) {
    lastTrigger = now;
    sendTrigger();
  }

  if (echoReady) {
    echoReady  = false;
    float dist = (echoDuration / 2.0) * 0.0343;

    // Faixa válida para recipientes pequenos (1-2L) — rejeita ecos
    // corrompidos/sem retorno em vez de aceitar qualquer leitura
    if (dist >= 2.0 && dist <= 40.0) {
      distSamples[distSampleIndex] = dist;
      distSampleIndex = (distSampleIndex + 1) % DIST_SAMPLES;
      if (distSampleCount < DIST_SAMPLES) distSampleCount++;

      currentDist     = medianDist();
      currentPercent  = calcPercent(currentDist);
      currentLiters   = cal.maxLiters * currentPercent / 100.0;
    }
  }

  if (now - lastMqttPub >= 5000) {
    lastMqttPub = now;
    publishData();
  }

  if (now - lastDisplayDraw >= 100) {
    lastDisplayDraw = now;

    switch (appState) {

      case S_MAIN:
        if (btnA.pressed) { appState = S_MENU; menuIndex = 0; }
        drawMainScreen();
        break;

      case S_MENU:
        if (btnA.pressed) menuIndex = (menuIndex + 1) % MENU_COUNT;
        if (btnB.pressed) handleMenuSelect();
        drawMenu();
        break;

      case S_CAL_FULL:
        if (btnA.pressed) appState = S_MENU;
        if (btnB.pressed) {
          cal.distFull = currentDist;
          saveCalibration();
          appState = S_MAIN;
        }
        drawCalConfirm(true);
        break;

      case S_CAL_EMPTY:
        if (btnA.pressed) appState = S_MENU;
        if (btnB.pressed) {
          cal.distEmpty = currentDist;
          saveCalibration();
          appState = S_MAIN;
        }
        drawCalConfirm(false);
        break;

      case S_CONFIG_LITERS:
        if (btnA.pressed) {
          configLitersTemp += 0.2;
          if (configLitersTemp > 2.0) configLitersTemp = 0.2;
        }
        if (btnB.pressed) {
          cal.maxLiters = configLitersTemp;
          saveCalibration();
          appState = S_MAIN;
        }
        drawConfigLiters();
        break;
    }

    btnA.pressed = false;
    btnB.pressed = false;
  }
}
