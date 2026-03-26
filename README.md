# Akwarium — sterownik ESP32

Szkic Arduino dla **ESP32** (np. ESP32-32E): odczyt czujników przez UART, sterowanie przekaźnikami z czasowym wyłączeniem oraz silnikiem DC przez mostek **BTS7960**.

## Wymagany sprzęt (zgodnie z kodem)

| Element | Opis |
|--------|------|
| **MCP23017** (I²C `0x20`) | Ekspander GPIO — **7 wyjść przekaźników** na pinach A0–A6 (w kodzie indeksy 0–6). |
| **ADS1115** (I²C `0x48`) | Przetwornik ADC — odczyt napięcia z kanału **A0** (np. czujnik poziomu, pH itp.). |
| **DS18B20** | Termometr na magistrali 1-Wire, pin **GPIO 32**. |
| **HC-SR04** | **Sześć** czujników odległości (ultradźwiękowych), każdy para TRIG/ECHO jak w `akwarium.ino`. |
| **BTS7960** | Sterownik silnika DC: **RPWM → GPIO 23**, **LPWM → GPIO 19** (PWM ~5 kHz, tylko kierunek „w prawo” w kodzie). |
| **I²C** | Domyślnie **SDA = 21**, **SCL = 22**, zegar 100 kHz. |

Przy starcie wykonywany jest **skan magistrali I²C** — na Serialu widać, które adresy odpowiadają (pomoc przy debugowaniu okablowania).

## Biblioteki Arduino

- `Wire`, `Adafruit_MCP23X17`, `Adafruit_ADS1X15`
- `OneWire`, `DallasTemperature`
- Na ESP32: `driver/ledc.h` (PWM silnika)

## Działanie programu

### `setup()`

1. Uruchamia **Serial** (115200 baud).
2. Inicjuje **I²C**, skanuje urządzenia, z opóźnieniami próbuje połączyć **MCP23017** i **ADS1115** (do 3 prób).
3. Konfiguruje **LEDC** dla dwóch kanałów PWM silnika (BTS7960).
4. Ustawia piny przekaźników na MCP jako wyjścia (stan niski).
5. Konfiguruje piny **6× HC-SR04** oraz inicjuje magistralę **DS18B20**.

Jeśli MCP lub ADS nie zostaną wykryte, program dalej działa, ale odpowiednie funkcje są pomijane (komunikaty na Serialu).

### `loop()` — w pętli bez przerwy

1. **Odczyt komend z Seriala** — linie zakończone `\n` lub `\r` są przekazywane do `processCommand()`.
2. **Pomiar odległości** dla D1–D6 (`measureDistance`). Brak echa lub błąd → w streamie jako **0** cm.
3. **Temperatura** z DS18B20 — błąd / brak czujnika → **0**.
4. **Napięcie** z ADS1115 (kanał 0), przeliczenie z próbki ADC — jeśli ADS nie działa, **0**.
5. **Automatyczne wyłączanie przekaźników** — jeśli wcześniej włączono je na określony czas, po upływie czasu wyjście MCP wraca do LOW.
6. **Jedna linia wyjściowa na Serial** ze wszystkimi wartościami (patrz niżej).

### Format danych wyjściowych (Serial)

Ciąg tekstowy, przykład:

```text
D1:12,D2:0,D3:45,D4:30,D5:0,D6:10,T:24.50,V:1.234
```

- **D1…D6** — odległość w cm (lub 0 przy błędzie).
- **T** — temperatura °C (2 miejsca po przecinku).
- **V** — napięcie z ADS1115 w woltach (3 miejsca po przecinku).

### Komendy wejściowe (Serial)

**Silnik DC** — prędkość w % (0–100), tylko jeden kierunek (kanał R w kodzie):

```text
DC:37
```

**Przekaźniki** — ramka w nawiasach klamrowych. Klucze `A0`…`A6` odpowiadają wyjściom MCP 0–6:

```text
{A0:0,A1:3,A2:5,A3:0,A4:1,A5:0,A6:9}
```

- Wartość **0** — przekaźnik wyłączony (natychmiast).
- Wartość **1–9** — przekaźnik włączony (HIGH) na **`wartość × 0,1 s`** (np. `A1:3` → 0,3 s), potem automatyczne wyłączenie.

Jest to **impulsowe** sterowanie: każda cyfra 1–9 ustala czas aktywnego stanu w jednostkach po 100 ms.

## Uwagi techniczne

- **HC-SR04**: `pulseIn` z timeoutem 20 ms ogranicza blokowanie przy braku odbicia.
- **Przekaźniki** wymagają działającego **MCP23017** — bez niego komendy ramki są ignorowane.
- **Silnik**: `setMotorSpeed` ogranicza zakres 0–100 % i mapuje na duty PWM 8-bit.

---

*Repozytorium: sterownik pod akwarium na ESP32.*
