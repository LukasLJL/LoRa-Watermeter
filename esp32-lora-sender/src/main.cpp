#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESP32Ping.h>
#include <HTTPClient.h>
#include <Ticker.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
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
void setupPreferences();
void setupLoRa();
void setupWiFi();
void setupTimer();
void setupWebServer();
String processor(const String &var);
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
  float current;
  float previous;
  String raw;
  String error;
  String rate;
  bool failed;
} WatermeterMetric;

typedef struct
{
  String ssid;
  String password;
  uint32_t interval;
  uint32_t word;
} Config;

Config config;

WatermeterMetric getWatermeterMetrics(String);

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

  setupPreferences();
  setupLoRa();
  setupWiFi();
  setupWebServer();
  setupTimer();
}

void setupPreferences()
{

  preferences.begin("settings", true);
  bool tpInit = preferences.isKey("hasInit");

  if (tpInit == false)
  {
    Serial.println("Inital preferences setup...");
    preferences.end();
    preferences.begin("settings", false);

    preferences.putString("wifi-ssid", WIFI_SSID);
    preferences.putString("wifi-password", WIFI_PASSWORD);
    preferences.putUInt("lora-interval", 10);
    preferences.putUInt("lora-word", 0xF3);

    preferences.putBool("hasInit", true);

    preferences.end();
    preferences.begin("settings", true);
  }

  Serial.println("Loading preferences");
  config.ssid = preferences.getString("wifi-ssid");
  config.password = preferences.getString("wifi-password");
  config.interval = preferences.getUInt("lora-interval");
  config.word = preferences.getUInt("lora-word");

  preferences.end();
}

void setupLoRa()
{
  while (!LoRa.begin(866E6))
  {
    Serial.println("Trying to start LoRa Interface");
    delay(500);
  }

  // Change sync word to match the receiver, ranges from 0-0xFF
  LoRa.setSyncWord(config.word);
  Serial.println("LoRa Initializing OK! With Sync Word " + String(config.word));
}

void setupWiFi()
{
  Serial.println("Starting WiFi-AP");

  WiFi.softAP(config.ssid, config.password);
  Serial.println("Started WiFi-AP with the SSID: " + config.ssid);

  Serial.print("AP-IP: ");
  Serial.println(WiFi.softAPIP());
}

void setupWebServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false, processor); });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/settings.html", String(), false, processor); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/style.css", "text/css"); });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {

    Config newConfig;

    if (request->hasParam("wifi-ssid", true)) {
        String ssid = request->getParam("wifi-ssid", true)->value();
        newConfig.ssid = ssid;
    }
    if (request->hasParam("wifi-ssid", true)) {
        String password = request->getParam("wifi-password", true)->value();
        newConfig.password = password;
    }
    if (request->hasParam("lora-interval", true)) {
        String interval = request->getParam("lora-interval", true)->value();
        newConfig.interval = static_cast<uint32_t>(interval.toInt());
    }
    if (request->hasParam("lora-word", true)) {
        String word = request->getParam("lora-word", true)->value();
        newConfig.word = static_cast<uint32_t>(word.toInt());
    }

    
    preferences.begin("settings", false);

    if (newConfig.ssid != ""){
      preferences.putString("wifi-ssid", newConfig.ssid);
    }
    if (newConfig.password != "") {
      preferences.putString("wifi-password", newConfig.password);
    }
    if (newConfig.interval) {
      preferences.putUInt("lora-interval", newConfig.interval);
    }
    if (newConfig.word) {
      preferences.putUInt("lora-word", newConfig.word);
    }
      
    preferences.end();
    
    request->send(200, "text/plain", "Saved settings. Rebooting...");
    delay(500);
    ESP.restart(); });

  server.begin();
}

void setupTimer()
{
  timer.attach_ms(1000 * config.interval, sendLoRa);
  Serial.println("Started LoRa Interval with " + String(config.interval) + " seconds.");
}

String processor(const String &var)
{
  if (var == "TEMPERATURE")
  {
    return String(dht.readTemperature());
  }
  else if (var == "HUMIDITY")
  {
    return String(dht.readHumidity());
  }
  else if (var == "WATERMETERIP")
  {
    return watermeterIP;
  }
  else if (var == "CONFIG_SSID")
  {
    return config.ssid;
  }
  else if (var == "CONFIG_PASSWORD")
  {
    return config.password;
  }
  else if (var == "CONFIG_INTERVAL")
  {
    return String(config.interval);
  }
  else if (var == "CONFIG_WORD")
  {
    return String(config.word);
  }
  return String();
}

void loop()
{
  if (watermeterIP == "127.0.0.1"){
    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;

    memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

    int count = adapter_sta_list.num;
    if (count > 0)
    {
      String ips[count];
      for (int i = 0; i < adapter_sta_list.num; i++)
      {

        tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];

        char ip[IP4ADDR_STRLEN_MAX];
        esp_ip4addr_ntoa(&station.ip, ip, IP4ADDR_STRLEN_MAX);
        ips[i] = ip;
      }
      watermeterIP = ips[0];
    }
  }
  delay(10000);
}

WatermeterMetric getWatermeterMetrics(String ip)
{
  WatermeterMetric metrics;
  HTTPClient http;
  String url = "http://" + ip + "/json";
  http.begin(url.c_str());

  int resCode = http.GET();

  if (resCode == 200)
  {
    String payload = http.getString();
    Serial.println(payload);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    metrics = WatermeterMetric{doc["main"]["value"],
                               doc["main"]["pre"],
                               doc["main"]["raw"],
                               doc["main"]["error"],
                               doc["main"]["rate"],
                               false};
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(resCode);
    metrics = WatermeterMetric{0, 0, "", "", "", true};
  }

  http.end();
  return metrics;
}

void sendLoRa()
{

  WatermeterMetric watermeterResult = getWatermeterMetrics(watermeterIP);

  // Manage LoRa-Payload
  StaticJsonDocument<192> payload;

  payload["humidity"] = dht.readHumidity();
  payload["temperature"] = dht.readTemperature();
  payload["packet_number"] = counter;

  JsonObject watermeter = payload.createNestedObject("watermeter");

  if (!watermeterResult.failed)
  {
    if (watermeterResult.current > watermeterResult.previous)
    {
      watermeter["current"] = watermeterResult.current;
    }
    watermeter["previous"] = watermeterResult.previous;
    watermeter["raw"] = watermeterResult.raw;
    watermeter["rate"] = watermeterResult.rate;
    watermeter["error"] = watermeterResult.error;
  }

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