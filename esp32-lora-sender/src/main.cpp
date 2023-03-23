#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <HTTPClient.h>
#include <Ticker.h>
#include <Preferences.h>
#include "SPIFFS.h"
#include "ESPAsyncWebServer.h"
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
#define DHTTYPE DHT22

// Functions
void setupLoRa();
void setupWiFi();
void setupTimer();
void setupWebServer();
String processor(const String& var);
String getWatermeterIP();
void sendLoRa();

// DHT
DHT dht(DHTPIN, DHTTYPE);

// Variables
int counter = 0;
String watermeterIP = "127.0.0.1";

Preferences preferences;
AsyncWebServer server(80);
Ticker timer;

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

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }


  // Setup Pin Configuration
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  dht.begin();

  setupLoRa();
  setupWiFi();
  setupWebServer();
  setupTimer();
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

void setupWebServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false, processor); });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/settings.html", String(), false); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/style.css", "text/css"); });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {

    
    String ssid;
    String password;
    String interval;
    String word;

    preferences.begin("settings", false);

    if (request->hasParam("wifi-ssid", true)) {
        ssid = request->getParam("wifi-ssid", true)->value();
    }
    if (request->hasParam("wifi-ssid", true)) {
        password = request->getParam("wifi-password", true)->value();
    }
    if (request->hasParam("lora-interval", true)) {
        interval = request->getParam("lora-interval", true)->value();
    }
    if (request->hasParam("lora-word", true)) {
        word = request->getParam("lora-word", true)->value();
    }

    if (ssid != "" && password != ""){
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
    }
      
    preferences.end();
    setupWiFi();
    
    request->send(200, "text/plain", "Added Settings"); });

  server.begin();
}

void setupTimer()
{
  timer.attach_ms(10000, sendLoRa); 
  Serial.println("Started LoRa Interval with 10");
}

String processor(const String& var){
  if(var == "TEMPERATURE"){
    return String(dht.readTemperature());
  }
  else if(var == "HUMIDITY"){
    return String(dht.readHumidity());
  }
  else if(var == "WATERMETERIP"){
    return watermeterIP;
  }
  return String();
}

void loop()
{

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

void sendLoRa()
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
}