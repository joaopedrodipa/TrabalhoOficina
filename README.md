# EcoSync SCADA — Módulo A: Telemetria de Volumetria

Projeto de Oficina de Integração 2 — UTFPR Campus Apucarana  
Engenharia da Computação

Monitora o nível de um reservatório com sensor ultrassônico HC-SR04, exibindo volume e porcentagem em um display OLED integrado. Envia dados via MQTT para um broker central.

---

## Hardware

| Componente | Modelo |
|---|---|
| Microcontrolador | ESP8266 NodeMCU ideaspark HW-364A |
| Display | OLED 0.96" SSD1306 integrado na placa |
| Sensor de distância | HC-SR04 |
| Botões | 2x push button 4 pinos |

---

## Mapa de pinos

| Componente | Pino do componente | Pino da placa |
|---|---|---|
| OLED | SDA / SCL | Integrado (GPIO14 / GPIO12) |
| HC-SR04 | VCC | VU (5V) |
| HC-SR04 | GND | G (GND) |
| HC-SR04 | TRIG | D2 (GPIO4) |
| HC-SR04 | ECHO | via divisor → D1 (GPIO5) |
| Botão A (Navegar) | pino diagonal | D7 e GND |
| Botão B (Confirmar) | pino diagonal | D3 e GND |

> **Atenção:** não segurar o Botão B ao ligar — D3/GPIO0 é o pino de boot.

---

## Esquemático

```
                    USB (5V)
                       │
┌──────────────────────┼──────────────────────────┐
│   ESP8266 HW-364A    │                          │
│                      │                          │
│   VU (5V) ───────────┼──────┬─── HC-SR04 VCC   │
│                      │      │                   │
│   G  (GND)───────────┼──┬───┼─── HC-SR04 GND   │
│                      │  │   │                   │
│   D2  ───────────────┼──┼───┼─── HC-SR04 TRIG  │
│                      │  │   │                   │
│                      │  │   │    HC-SR04 ECHO   │
│                      │  │   │         │         │
│                      │  │   │        [1kΩ]      │
│                      │  │   │         │         │
│   D1  ───────────────┼──┼───┼─────────┤         │
│                      │  │   │        [2kΩ]      │
│                      │  │   │         │         │
│   G  (GND)───────────┼──┴───┴─────────┘         │
│                      │                          │
│                      │   BTN A (4 pinos)        │
│   D7  ───────────────┼───────┤A  B├             │
│                      │       │    │             │
│   G  (GND)───────────┼───────┤D  C├             │
│                      │                          │
│                      │   BTN B (4 pinos)        │
│   D3  ───────────────┼───────┤A  B├             │
│                      │       │    │             │
│   G  (GND)───────────┼───────┤D  C├             │
│                      │                          │
└──────────────────────┼──────────────────────────┘
                       │
                    PC / Carregador
```

### Divisor de tensão (obrigatório no ECHO)

O HC-SR04 retorna 5V no pino ECHO, mas o ESP8266 suporta no máximo 3.6V nos GPIOs. O divisor reduz para ~3.3V.

```
HC-SR04 ECHO ──[1kΩ]──┬── D1 (ESP)
                      [2kΩ]
                       │
                      GND
```

A proporção 1:2 é o que importa. Exemplos equivalentes:

| R1 | R2 | Tensão resultante |
|---|---|---|
| 1kΩ | 2kΩ | 3.33V |
| 2.2kΩ | 4.7kΩ | 3.37V |
| 10kΩ | 20kΩ | 3.33V |

Verificar com multímetro: deve marcar entre **2.5V e 3.5V** no ponto entre os resistores.

---

## Bibliotecas necessárias (Arduino IDE)

Instalar via `Sketch > Manage Libraries`:

- **Adafruit SSD1306** (instalar com todas as dependências)
- **Adafruit GFX Library** (instalada automaticamente)
- **Adafruit BusIO** (instalada automaticamente)

Suporte à placa ESP8266 via `File > Preferences > Additional boards manager URLs`:
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Placa: `Tools > Board > esp8266 > NodeMCU 1.0 (ESP-12E Module)`

---

## Configuração antes de usar

Editar no `firmware/ecosync/ecosync.ino`:

```cpp
const char* WIFI_SSID     = "SEU_WIFI";   // nome da rede 2.4GHz
const char* WIFI_PASSWORD = "SUA_SENHA";  // senha da rede
```

---

## Como usar

1. Fazer upload do firmware pelo Arduino IDE
2. A tela de boot exibe **"EcoSync SCADA / Volumetria v1.0"**
3. Calibrar o sensor:
   - Reservatório **vazio** → Botão A → "Calibrar VAZIO" → Botão B para salvar
   - Reservatório **cheio** → Botão A → "Calibrar CHEIO" → Botão B para salvar
4. Configurar volume máximo: Menu → "Config. Litros" → Botão A (+0.5L) → Botão B para salvar
5. Tela principal exibe nível em % e litros em tempo real

### Status do display

| Status | Faixa |
|---|---|
| NORMAL | acima de 60% |
| BAIXO | entre 25% e 60% |
| CRITICO | abaixo de 25% |

---

## Estrutura do projeto

```
TrabalhoOficina/
├── firmware/
│   └── ecosync/
│       └── ecosync.ino    ← firmware do ESP8266
├── frontend/
│   └── index.html         ← dashboard web (aguardando API do professor)
└── README.md
```

---

## JSON publicado via MQTT

```json
{
  "nivel_pct":    73.5,
  "volume_l":     14.7,
  "distancia_cm": 12.3,
  "status":       "NORMAL"
}
```

Tópico: `ecosync/volumetria`

---

## Pendente

- [ ] Receber biblioteca MQTT do professor → integrar nos `TODO` do firmware
- [ ] Receber URL e parâmetros da API → atualizar `API_BASE` no frontend
- [ ] Colocar credenciais WiFi reais no firmware
