#define BLYNK_TEMPLATE_ID "TMPL3l-cV_faE"
#define BLYNK_TEMPLATE_NAME "EGG PROJECT"
#define BLYNK_AUTH_TOKEN "VaehU3dWibk_er-uyMH9HUT6nHznn4dS"

#include "DHT.h"
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>

// WIFI 
const char* ssid = "Redmi Note 12 Pro+ 5G";
const char* password = "12345678";
String serverIP = "10.27.26.233";

// PINS
#define DHTPIN 4
#define DHTTYPE DHT11
#define SDA_PIN 21
#define SCL_PIN 22
#define LED_PIN 2
#define NH3_PIN 34

// SENSOR SETTINGS 
#define MAX44009_ADDR 0x4A
#define LUX_HIGH_BYTE 0x03
#define LUX_LOW_BYTE  0x04
#define VCC_VOLTAGE 3.3
#define ADC_RES 4095.0
#define RL 10.0
#define RO_CLEAN_AIR 90.29

DHT dht(DHTPIN, DHTTYPE);
float g_temp, g_hum, g_lux, g_nh3;
int day = 10;

// SETUP 
void setup() {
  Serial.begin(115200);
  dht.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  analogReadResolution(12);
  Serial.println("ESP32 Sensor + WiFi + Blynk System");

  // WiFi + Blynk
  WiFi.begin(ssid, password);
  Blynk.config(BLYNK_AUTH_TOKEN);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
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
//DHT
void readDHT11() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    g_temp = t;
    g_hum = h;
    Serial.printf("[DHT11] %.1f°C | %.1f%%\n", t, h);
  }
}
//LUX 
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
  if (Wire.endTransmission() != 0) return -1;
  Wire.requestFrom(MAX44009_ADDR, 1);
  if (!Wire.available()) return -1;
  uint8_t highByte = Wire.read();
  Wire.beginTransmission(MAX44009_ADDR);
  Wire.write(LUX_LOW_BYTE);
  if (Wire.endTransmission() != 0) return -1;
  Wire.requestFrom(MAX44009_ADDR, 1);
  if (!Wire.available()) return -1;
  uint8_t lowByte = Wire.read();
  int exponent = (highByte >> 4) & 0x0F;
  int mantissa = ((highByte & 0x0F) << 4) | (lowByte & 0x0F);
  return pow(2, exponent) * mantissa * 0.045;
}
//NH3 
void readNH3() {
  float voltage = (analogRead(NH3_PIN) / ADC_RES) * VCC_VOLTAGE;
  if (voltage > 0) {
    float rs = ((VCC_VOLTAGE - voltage) / voltage) * RL;
    float ratio = rs / RO_CLEAN_AIR;
    float ppm = 102.2 * pow(ratio, -2.473);
    g_nh3 = ppm;
    Serial.printf("[NH3] %.2f ppm\n", ppm);
  }
}
//SEND TO FLASK + BLYNK
void sendToServer() {
  //Auto reconnect WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting WiFi...");
    WiFi.begin(ssid, password);
    delay(2000);
    return;
  }
  HTTPClient http;
  String url = "http://" + serverIP + ":5001/predict?";
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

    //SEND TO BLYNK
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
