#include "DHT.h"
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>

// User Configuration
#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "YOUR_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_AUTH_TOKEN"

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

String SERVER_IP = "YOUR_SERVER_IP";

int day = 10;

// Pin Definitions
#define DHTPIN 4
#define DHTTYPE DHT11
#define SDA_PIN 21
#define SCL_PIN 22
#define LED_PIN 2
#define NH3_PIN 34

// Sensor Configuration
#define MAX44009_ADDR 0x4A
#define LUX_HIGH_BYTE 0x03
#define LUX_LOW_BYTE  0x04
#define VCC_VOLTAGE 3.3
#define ADC_RES 4095.0
#define RL 10.0
#define RO_CLEAN_AIR 90.29

DHT dht(DHTPIN, DHTTYPE);

float g_temp = 0;
float g_hum = 0;
float g_lux = 0;
float g_nh3 = 0;

void setup() {
  Serial.begin(115200);

  dht.begin();
  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  analogReadResolution(12);

  Serial.println("ESP32 Sensor + WiFi + Blynk System");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Blynk.config(BLYNK_AUTH_TOKEN);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Blynk.connect();
}

void loop() {
  Blynk.run();

  Serial.println("\n-------------------------------------");

  readDHT11();
  readMAX44009();
  readNH3();

  sendToServer();

  Serial.println("-------------------------------------");

  delay(3000);
}

void readDHT11() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {
    g_temp = t;
    g_hum = h;

    Serial.printf("[DHT11] %.1f°C | %.1f%%\n", t, h);
  }
}

void readMAX44009() {
  float lux = readLux();

  if (lux >= 0) {
    g_lux = lux;
    Serial.printf("[LUX] %.2f\n", lux);
  }
}

float readLux() {
  Wire.beginTransmission(MAX44009_ADDR);
  Wire.write(LUX_HIGH_BYTE);

  if (Wire.endTransmission() != 0)
    return -1;

  Wire.requestFrom(MAX44009_ADDR, 1);

  if (!Wire.available())
    return -1;

  uint8_t highByte = Wire.read();

  Wire.beginTransmission(MAX44009_ADDR);
  Wire.write(LUX_LOW_BYTE);

  if (Wire.endTransmission() != 0)
    return -1;

  Wire.requestFrom(MAX44009_ADDR, 1);

  if (!Wire.available())
    return -1;

  uint8_t lowByte = Wire.read();

  int exponent = (highByte >> 4) & 0x0F;
  int mantissa = ((highByte & 0x0F) << 4) | (lowByte & 0x0F);

  return pow(2, exponent) * mantissa * 0.045;
}

void readNH3() {
  float voltage = (analogRead(NH3_PIN) / ADC_RES) * VCC_VOLTAGE;

  if (voltage > 0) {
    float rs = ((VCC_VOLTAGE - voltage) / voltage) * RL;
    float ratio = rs / RO_CLEAN_AIR;

    g_nh3 = 102.2 * pow(ratio, -2.473);

    Serial.printf("[NH3] %.2f ppm\n", g_nh3);
  }
}

void sendToServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting WiFi...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    delay(2000);

    return;
  }

  HTTPClient http;

  String url = "http://" + SERVER_IP + ":5001/predict?";
  url += "nh3=" + String(g_nh3);
  url += "&lux=" + String(g_lux);
  url += "&temp=" + String(g_temp);
  url += "&hum=" + String(g_hum);
  url += "&day=" + String(day);

  Serial.println("Sending:");
  Serial.println(url);

  http.begin(url);

  int httpCode = http.GET();

  if (httpCode > 0) {
    String response = http.getString();

    Serial.println("Prediction:");
    Serial.println(response);

    Blynk.virtualWrite(V0, g_nh3);
    Blynk.virtualWrite(V1, g_lux);
    Blynk.virtualWrite(V2, g_temp);
    Blynk.virtualWrite(V3, g_hum);
    Blynk.virtualWrite(V4, response);
  } else {
    Serial.println("HTTP Error");
  }

  http.end();
}