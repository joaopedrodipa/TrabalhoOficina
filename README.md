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

OLED é integrado na placa (SDA=GPIO14/D5, SCL=GPIO12/D6) — não precisa fiação.

### HC-SR04 (sensor ultrassônico)

| Pino do HC-SR04 | Conecta em |
|---|---|
| VCC | VU |
| TRIG | D2 |
| ECHO | resistor 1.2kΩ → nó do divisor → D1 |
| GND | G (via trilho GND da protoboard) |

### Botão A (Navegar)

| Pino do Botão A | Conecta em |
|---|---|
| Pino diagonal 1 | D7 |
| Pino diagonal 2 | G (via trilho GND da protoboard) |

### Botão B (Confirmar)

| Pino do Botão B | Conecta em |
|---|---|
| Pino diagonal 1 | D3 |
| Pino diagonal 2 | G (via trilho GND da protoboard) |

> **Atenção:** não segurar o Botão B ao ligar — D3/GPIO0 é o pino de boot.

### Divisor de tensão (componente próprio)

| Ponto | Conecta em |
|---|---|
| Resistor 1.2kΩ (entrada) | ECHO do HC-SR04 |
| Nó entre os resistores | D1 |
| Resistor 2.2kΩ (saída) | G (via trilho GND da protoboard) |

### ESP8266 (visão pela placa)

| Pino do ESP | Recebe de |
|---|---|
| VU | VCC do HC-SR04 |
| D2 | TRIG do HC-SR04 |
| D1 | nó do divisor de tensão (ECHO reduzido) |
| D7 | Botão A (pino diagonal 1) |
| D3 | Botão B (pino diagonal 1) |
| G | ligado ao trilho GND da protoboard — ponto comum de retorno de tudo |

> Ligue **um único fio** do pino G do ESP ao trilho negativo (−) da protoboard. Todos os outros GND (HC-SR04, divisor, botões) conectam nesse trilho, em vez de empilhar fios direto no pino G da placa.

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

A proporção ~1:2 é o que importa (R1 perto do ECHO, R2 perto do GND). Exemplos equivalentes:

| R1 (ECHO→nó) | R2 (nó→GND) | Tensão resultante |
|---|---|---|
| 1kΩ | 2kΩ | 3.33V |
| 1.2kΩ | 2.2kΩ | 3.24V |
| 2.2kΩ | 4.7kΩ | 3.37V |
| 10kΩ | 20kΩ | 3.33V |

⚠️ Se inverter a ordem dos resistores a tensão cai demais (ex: 1.2k/2.2k invertido dá ~1.76V — sinal instável).

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
