#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_ADS1X15.h>
#ifdef ESP32
  #include "driver/ledc.h"
#endif

Adafruit_MCP23X17 mcp;
Adafruit_ADS1115 ads;

bool mcpOk = false;   // true jeśli MCP23017 wykryty
bool adsOk = false;   // true jeśli ADS1115 wykryty

String uartBuffer = "";

// ===== RELAYS A0-A6 (MCP23017) =====
const int relayCount = 7;

unsigned long relayTimer[relayCount];
unsigned long relayDuration[relayCount];
bool relayActive[relayCount];

const unsigned long relayUnitTime = 100; // 0.1 sekundy

// ===== DS18B20 =====
#define ONE_WIRE_BUS 32
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ===== BTS7960 DC MOTOR DRIVER =====
const int motorRPWM = 23;  // PWM pin dla kierunku w prawo
const int motorLPWM = 19;  // PWM pin dla kierunku w lewo
#ifdef ESP32
  const int motorChannelR = 0;
  const int motorChannelL = 1;
#endif
const int motorFreq = 5000;   // Częstotliwość PWM (5kHz)

// ===== HC-SR04 (6 czujników – bez pinu 0) =====
const int trigPin1 = 25; const int echoPin1 = 33;
const int trigPin2 = 27; const int echoPin2 = 26;
const int trigPin3 = 12; const int echoPin3 = 14;
const int trigPin4 = 13; const int echoPin4 = 15;
const int trigPin5 = 4;  const int echoPin5 = 16;
const int trigPin6 = 7;  const int echoPin6 = 18;

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  // I2C: na większości płytek ESP32 SDA=21, SCL=22 (Wire.begin(SDA, SCL))
  const int I2C_SDA = 21;
  const int I2C_SCL = 22;
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);   // 100 kHz – bardziej niezawodne przy długich kablach / słabych pull-upach
  delay(100);             // chwila na ustabilizowanie magistrali

  // Skan I2C – wypisuje, co jest na magistrali
  Serial.print("I2C Scanner (SDA="); Serial.print(I2C_SDA); Serial.print(", SCL="); Serial.print(I2C_SCL); Serial.println("):");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Znaleziono: 0x"); Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  Brak urzadzen I2C!");
    Serial.println("  Sprawdz: GND wspolne, SDA->GPIO21, SCL->GPIO22, pull-upy 4.7k do 3.3V.");
  }

  delay(150);   // chwila na ustabilizowanie magistrali po skanie

  // MCP23017 (adres 0x20) – do 3 prób, skan czasem „zajmuje” magistralę
  for (int i = 0; i < 3 && !mcpOk; i++) {
    if (i > 0) delay(120);
    mcpOk = mcp.begin_I2C(0x20);
  }
  if (!mcpOk) Serial.println("Brak MCP23017 (0x20)");

  // ADS1115 (0x48) – do 3 prób
  for (int i = 0; i < 3 && !adsOk; i++) {
    if (i > 0) delay(120);
    adsOk = ads.begin();
  }
  if (!adsOk) Serial.println("Brak ADS1115 (0x48)");

  if (adsOk) ads.setGain(GAIN_ONE); // ±4.096 V

  // Konfiguracja BTS7960 DC Motor Driver (ESP32 LEDC)
  #ifdef ESP32
    pinMode(motorRPWM, OUTPUT);
    pinMode(motorLPWM, OUTPUT);
    ledc_timer_config_t tR = {};
    tR.speed_mode = LEDC_LOW_SPEED_MODE;
    tR.duty_resolution = LEDC_TIMER_8_BIT;
    tR.timer_num = LEDC_TIMER_0;
    tR.freq_hz = motorFreq;
    tR.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&tR);
    ledc_timer_config_t tL = {};
    tL.speed_mode = LEDC_LOW_SPEED_MODE;
    tL.duty_resolution = LEDC_TIMER_8_BIT;
    tL.timer_num = LEDC_TIMER_1;
    tL.freq_hz = motorFreq;
    tL.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&tL);
    ledc_channel_config_t chR = {};
    chR.gpio_num = (gpio_num_t)motorRPWM;
    chR.speed_mode = LEDC_LOW_SPEED_MODE;
    chR.channel = (ledc_channel_t)motorChannelR;
    chR.timer_sel = LEDC_TIMER_0;
    chR.duty = 0;
    chR.hpoint = 0;
    ledc_channel_config(&chR);
    ledc_channel_config_t chL = {};
    chL.gpio_num = (gpio_num_t)motorLPWM;
    chL.speed_mode = LEDC_LOW_SPEED_MODE;
    chL.channel = (ledc_channel_t)motorChannelL;
    chL.timer_sel = LEDC_TIMER_1;
    chL.duty = 0;
    chL.hpoint = 0;
    ledc_channel_config(&chL);
  #endif

  // Konfiguracja przekaźników (tylko jeśli MCP jest OK)
  for (int i = 0; i < relayCount; i++) {
    relayActive[i] = false;
    relayTimer[i] = 0;
    relayDuration[i] = 0;
    if (mcpOk) {
      mcp.pinMode(i, OUTPUT);
      mcp.digitalWrite(i, LOW);
    }
  }

  // HC-SR04 (6 czujników)
  pinMode(trigPin1, OUTPUT); pinMode(echoPin1, INPUT);
  pinMode(trigPin2, OUTPUT); pinMode(echoPin2, INPUT);
  pinMode(trigPin3, OUTPUT); pinMode(echoPin3, INPUT);
  pinMode(trigPin4, OUTPUT); pinMode(echoPin4, INPUT);
  pinMode(trigPin5, OUTPUT); pinMode(echoPin5, INPUT);
  pinMode(trigPin6, OUTPUT); pinMode(echoPin6, INPUT);

  sensors.begin();

  Serial.println("System gotowy");
}

// ================= LOOP =================
void loop() {

  // ===== UART INPUT =====
  while (Serial.available()) {

    char c = Serial.read();

    if (c == '\n' || c == '\r') {

      if (uartBuffer.length() > 0) {
        processCommand(uartBuffer);
        uartBuffer = "";
      }
    }
    else {
      uartBuffer += c;
    }
  }

  // ===== HC-SR04 (6 czujników) – brak echa / brak czujnika → 0 =====
  long d1 = measureDistance(trigPin1, echoPin1);
  long d2 = measureDistance(trigPin2, echoPin2);
  long d3 = measureDistance(trigPin3, echoPin3);
  long d4 = measureDistance(trigPin4, echoPin4);
  long d5 = measureDistance(trigPin5, echoPin5);
  long d6 = measureDistance(trigPin6, echoPin6);
  if (d1 < 0) d1 = 0;
  if (d2 < 0) d2 = 0;
  if (d3 < 0) d3 = 0;
  if (d4 < 0) d4 = 0;
  if (d5 < 0) d5 = 0;
  if (d6 < 0) d6 = 0;

  // ===== DS18B20 – brak czujnika / błąd → 0 =====
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  if (temp != temp || temp < -100.0f) temp = 0.0f;

  // ===== ADS1115 – brak modułu / nie wykryty → 0 =====
  float voltage = 0.0f;
  if (adsOk) {
    int16_t adc0 = ads.readADC_SingleEnded(0);
    voltage = adc0 * 0.125f / 1000.0f;
  }

  // ===== AUTO OFF RELAYS =====
  if (mcpOk) {
    for (int i = 0; i < relayCount; i++) {
      if (relayActive[i] && millis() - relayTimer[i] >= relayDuration[i]) {
        mcp.digitalWrite(i, LOW);
        relayActive[i] = false;
      }
    }
  }

  // ===== UART OUTPUT (brak czujnika / błąd = 0) =====
  Serial.print("D1:"); Serial.print(d1);
  Serial.print(",D2:"); Serial.print(d2);
  Serial.print(",D3:"); Serial.print(d3);
  Serial.print(",D4:"); Serial.print(d4);
  Serial.print(",D5:"); Serial.print(d5);
  Serial.print(",D6:"); Serial.print(d6);
  Serial.print(",T:");  Serial.print(temp, 2);
  Serial.print(",V:");  Serial.println(voltage, 3);
}

// ================= ODBIÓR KOMEND =================
// Format: {A0:0,A1:3,A2:5,A3:0,A4:1,A5:0,A6:9}

void processCommand(String cmd) {

  cmd.trim();

  // Obsługa komendy DC Motor: "DC:37"
  if (cmd.startsWith("DC:")) {
    int percent = cmd.substring(3).toInt();
    setMotorSpeed(percent);
    return;
  }

  // Obsługa komend przekaźników: {A0:0,A1:3,...}
  if (!cmd.startsWith("{") || !cmd.endsWith("}"))
    return;
  if (!mcpOk) return;

  cmd.remove(0, 1);
  cmd.remove(cmd.length() - 1);

  for (int i = 0; i < relayCount; i++) {

    String key = "A" + String(i) + ":";
    int index = cmd.indexOf(key);

    if (index != -1) {

      int value = cmd.charAt(index + key.length()) - '0';

      if (value > 0 && value <= 9) {

        mcp.digitalWrite(i, HIGH);

        relayActive[i] = true;
        relayTimer[i] = millis();
        relayDuration[i] = value * relayUnitTime;
      }
      else {

        mcp.digitalWrite(i, LOW);
        relayActive[i] = false;
      }
    }
  }
}

// ================= BTS7960 DC MOTOR CONTROL =================
void setMotorSpeed(int percent) {
  percent = constrain(percent, 0, 100);
  int pwmValue = map(percent, 0, 100, 0, 255);
  #ifdef ESP32
    if (percent > 0) {
      ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)motorChannelR, pwmValue);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)motorChannelR);
      ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)motorChannelL, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)motorChannelL);
    } else {
      ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)motorChannelR, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)motorChannelR);
      ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)motorChannelL, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)motorChannelL);
    }
  #endif
}

// ================= HC-SR04 =================
long measureDistance(int trigPin, int echoPin) {

  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 20000);  // 20 ms – mniej blokowania gdy brak echa

  if (duration == 0)
    return -1;

  return duration * 0.034 / 2;
}
