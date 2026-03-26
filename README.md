

## Biblioteki esp32
1.  **Adafruit_MCP23X17** - ekspander umożliwiający podpięcie przekaźników
2.  **Adafruit_ADS1X15**, biblioteka obsługująca przetwornik.
3.  **OneWire** umozliwia odczyt danych z kilku urządzeń z I2C
4. **DallasTemperature** służy do sterowania czujnikiem temepratury DS18B20.



## Działanie programu

### `setup()`

1. Uruchamia **Serial** (115200 baud).
2. Inicjuje **I²C**, skanuje urządzenia, z opóźnieniami próbuje połączyć **MCP23017** i **ADS1115** (do 3 prób).
3. Konfiguruje **LEDC** dla dwóch kanałów PWM silnika (BTS7960).
4. Ustawia piny przekaźników na MCP jako wyjścia (stan niski).
5. Konfiguruje piny **6× HC-SR04** oraz inicjuje magistralę **DS18B20**.



### `loop()` — w pętli bez przerwy

1. **Odczyt komend z UART**
2. **Pomiar odległości** 
3. **Temperatura** z DS18B20 — błąd / brak czujnika → **0**.
4. **Pomiar przypływu** przeliczane z ADS1115 
5. **Automatyczne wyłączanie przekaźników** — jeśli wcześniej włączono je na określony czas, po upływie czasu wracają do stanu 0


### Format danych wyjściowych (UART)

Ciąg tekstowy, przykład:

D1:12,D2:0,D3:45,D4:30,D5:0,D6:10,T:24.50,V:1.234


- **D1…D6** — odległość w cm (lub 0 przy błędzie).
- **T** — temperatura °C (2 miejsca po przecinku).
- **V** — napięcie z ADS1115 w woltach (3 miejsca po przecinku).

### Komendy wejściowe (Serial)

**Silnik DC** — prędkość w % (0–100), tylko jeden kierunek 


**Przekaźniki** — ramka w nawiasach klamrowych. Klucze `A0`…`A6` odpowiadają wyjściom MCP 0–6:



- Wartość **0** — przekaźnik wyłączony (natychmiast).
- Wartość **1–9** — przekaźnik włączony  na **`wartość × 0,1 s`** (np. `A1:3` → 0,3 s), potem automatyczne wyłączenie.

