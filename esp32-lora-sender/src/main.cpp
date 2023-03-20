#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <HTTPClient.h>
#include "DHT.h"
#include "secrets.h"

// LoRa Pins
#define SS 18
#define RST 14
#define DIO0 26

// SPI Pins
#define SCK 5
#define MISO 19
#define MOSI 27

// Sensor Config
#define DHTPIN 13
#define DHTTYPE DHT11

// Functions
void setupLoRa();
void setupWiFi();
String getWatermeterIP();

// DHT
DHT dht(DHTPIN, DHTTYPE);

// Variables
int counter = 0;
String watermeterIP = "127.0.0.1";

typedef struct
{
  int current;
  int previous;
} watermeterMetric;

watermeterMetric getWatermeterMetrics(String);

void setup()
{
  Serial.begin(115200);
  Serial.println("LoRa Sender");

  // Setup Pin Configuration
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  dht.begin();

  setupLoRa();
  setupWiFi();
}

void setupLoRa()
{
  while (!LoRa.begin(866E6))
  {
    Serial.println("Trying to start LoRa Interface");
    delay(500);
  }

  // Change sync word to match the receiver, ranges from 0-0xFF
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");
}

void setupWiFi()
{
  Serial.println("Starting WiFi-AP");
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Started WiFi-AP with the SSID: " + String(WIFI_SSID));
  Serial.print("AP-IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop()
{
  // Caching Watermeter IP-Address
  if (watermeterIP == "127.0.0.1")
  {
    watermeterIP = getWatermeterIP();
  }

  watermeterMetric watermeterResult = getWatermeterMetrics(watermeterIP);

  // Manage LoRa-Payload
  StaticJsonDocument<96> payload;

  payload["humidity"] = dht.readHumidity();
  payload["temperature"] = dht.readTemperature();
  payload["packet_number"] = counter;
  payload["current-meter"] = watermeterResult.current;
  payload["pre-meter"] = watermeterResult.previous;
  payload["message"] = "Hello";

  String payloadSerialized;
  serializeJson(payload, payloadSerialized);

  Serial.print("Sending packet: ");
  Serial.println(payloadSerialized);

  // Send LoRa packet to receiver
  LoRa.beginPacket();
  LoRa.print(payloadSerialized);
  LoRa.endPacket();

  counter++;

  delay(10000);
}

watermeterMetric getWatermeterMetrics(String ip)
{
  HTTPClient http;
  String url = ip + "/json";
  http.begin(url.c_str());

  int resCode = http.GET();

  if (resCode == 200)
  {
    String payload = http.getString();
    Serial.println(payload);
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(resCode);
  }

  http.end();
  return watermeterMetric{1337, 1336};
}

String getWatermeterIP()
{
  for (int i = 2; i <= 254; i++)
  {
    IPAddress ip(192, 168, 4, i);
    bool res = Ping.ping(ip);
    if (res)
    {
      Serial.println("Found WiFi-Device with IP: " + ip.toString());
      return ip.toString();
    }
  }
  return "127.0.0.1";
}
