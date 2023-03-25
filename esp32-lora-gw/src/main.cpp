#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include "secrets.h"

// LoRa Pins
#define SS 18
#define RST 14
#define DIO0 26

// SPI Pins
#define SCK 5
#define MISO 19
#define MOSI 27

// MQTT Topics/Channels
#define mqttChannel "esp32-lora-gw"
#define mqttStatus mqttChannel "/status"
#define mqttSensor mqttChannel "/sensor"
#define mqttWaterMeter mqttChannel "/watermeter"
#define mqttState mqttChannel "/state"

// WebConfig Fields
#define wifiSSID "wifi-ssid"
#define wifiPassword "wifi-password"
#define mqttHost "mqtt-host"
#define mqttPort "mqtt-port"
#define mqttUser "mqtt-user"
#define mqttPassword "mqtt-password"
#define mqttClient "mqtt-client"
#define loraSync "lora-sync"

// Functions
void setupLoRa();
void setupWiFi();
void setupMQTT();
void setupWebServer();
void reconnect();
void sendHomeAssistantDiscovery();
void MQTTHomeAssistantDiscovery();
void sendDeviceInformationMQTT();
String processor(const String &var);

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Variables
unsigned long previousMillis = 0;
long one_minute_interval = 1 * 60 * 1000UL;

Preferences preferences;
AsyncWebServer server(80);

struct homeAssistantTopic
{
  String field;
  String name;
  String icon;
  String unit;
  String deviceClass;
  String stateClass;
  String entityCategory;
};

void setup()
{
  Serial.begin(115200);
  Serial.println("LoRa Gateway");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Setup Pin Configuration
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  setupLoRa();
  setupWiFi();
  setupWebServer();
  setupMQTT();

  MQTTHomeAssistantDiscovery();
  sendDeviceInformationMQTT();
}

void setupLoRa()
{
  while (!LoRa.begin(866E6))
  {
    Serial.println("Trying to start LoRa Interface");
    delay(500);
  }

  // Change sync word to match the receiver, ranges from 0-0xFF
  preferences.begin("lora-settings", true);
  String syncWord = preferences.getString("sync", "");
  preferences.end();

  if (syncWord == "")
  {
    syncWord = "0xF3";
  }

  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");
}

void setupWiFi()
{
  String ssid;
  String password;

  preferences.begin("wifi-settings", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();

  if (ssid == "" || password == "")
  {
    Serial.println("Creating WiFi-AP to setup device...");
    WiFi.softAP(INIT_WIFI_SSID, INIT_WIFI_PASSWORD);
    Serial.println("Started WiFi-AP with the SSID: " + String(INIT_WIFI_SSID));
  }
  else
  {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid.c_str(), password.c_str());
    WiFi.setHostname("esp32-lora-gw");
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.println("Trying to connect to your WiFi...");
    }
    Serial.print("ESP32 IP-Address: ");
    Serial.print(WiFi.localIP());
    Serial.println("");
  }
}

void setupMQTT()
{
  preferences.begin("mqtt-settings", true);
  String host = preferences.getString("host", "");
  int port = preferences.getInt("port", 1883);
  String user = preferences.getString("user", "");
  String password = preferences.getString("password", "");
  String mqtt_client = preferences.getString("client", "ESP32-LoRa-GW");
  preferences.end();

  if (host != "" && user != "" && password != "")
  {
    client.setServer(host.c_str(), port);
    client.setBufferSize(1024);
    while (!client.connected())
    {
      Serial.println("Attempting MQTT connection...");
      if (client.connect(mqtt_client.c_str(), user.c_str(), password.c_str()))
      {
        Serial.println("MQTT connected with name: " + String(mqtt_client));
      }
      else
      {
        Serial.println("MQTT connection failed.");
      }
    }
    client.publish(mqttStatus, "connected", true);
  }
  else
  {
    Serial.println("No MQTT-Server configured!");
  }
}

void setupWebServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false); });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/settings.html", String(), false, processor); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/style.css", "text/css"); });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    // WiFi
    preferences.begin("wifi-settings", false);
    if (request->hasParam(wifiSSID, true)) {
      preferences.putString("ssid", request->getParam(wifiSSID, true)->value());
    }
    if (request->hasParam(wifiPassword, true)) {
      preferences.putString("password", request->getParam(wifiPassword, true)->value());
    }  
    preferences.end();

    //MQTT
    preferences.begin("mqtt-settings", false);
    if (request->hasParam(mqttHost, true)) {
      preferences.putString("host", request->getParam(mqttHost, true)->value());
    }
    if (request->hasParam(mqttPort, true)) {
      preferences.putInt("port", request->getParam(mqttPort, true)->value().toInt());
    }
    if (request->hasParam(mqttUser, true)) {
      preferences.putString("user", request->getParam(mqttUser, true)->value());
    }
    if (request->hasParam(mqttPassword, true)) {
      preferences.putString("password", request->getParam(mqttPassword, true)->value());
    }
    if (request->hasParam(mqttClient, true)) {
      preferences.putString("client", request->getParam(mqttClient, true)->value());
    } 
    preferences.end();

    //LoRa
    preferences.begin("lora-settings", false);
    if (request->hasParam(loraSync, true)) {
      preferences.putString("sync", request->getParam(loraSync, true)->value());
    }
    preferences.end();

    //Restart
    ESP.restart();
    
    request->send(200, "text/plain", "Changed System Settings"); });

  server.begin();
}

String processor(const String &var)
{
  // WiFi-Properties
  preferences.begin("wifi-settings", true);
  if (var == "WIFI-SSID")
  {
    return String(preferences.getString("ssid", ""));
  }
  else if (var == "WIFI-PASSWORD")
  {
    return String(preferences.getString("password", ""));
  }
  preferences.end();

  // MQTT-Properties
  preferences.begin("mqtt-settings", true);
  if (var == "MQTT-HOST")
  {
    return String(preferences.getString("host", ""));
  }
  else if (var == "MQTT-PORT")
  {
    return String(preferences.getInt("port", 1883));
  }
  else if (var == "MQTT-USER")
  {
    return String(preferences.getString("user", ""));
  }
  else if (var == "MQTT-PASSWORD")
  {
    return String(preferences.getString("password", ""));
  }
  else if (var == "MQTT-CLIENT")
  {
    return String(preferences.getString("client", ""));
  }
  preferences.end();

  // LoRa-Properties
  preferences.begin("lora-settings", true);
  if (var == "LORA-SYNC")
  {
    return String(preferences.getString("sync", ""));
  }
  preferences.end();

  return String();
}

void sendHomeAssistantDiscovery(homeAssistantTopic haTopic)
{
  const int capacityPayload = JSON_OBJECT_SIZE(26);
  StaticJsonDocument<capacityPayload> payload;

  String mainTopic = mqttChannel;
  String configTopic = "homeassistant/sensor/" + mainTopic + "/" + haTopic.field + "/config";

  JsonObject device = payload.createNestedObject("device");
  device["identifiers"] = mainTopic;
  device["model"] = "LoRa-Watermeter";
  device["manufacturer"] = "IoT";
  device["name"] = "LoRa-Watermeter";
  device["sw_version"] = "0.0.1";

  payload["~"] = mainTopic;
  payload["unique_id"] = "esp32-lora-gw-" + haTopic.field;
  payload["object_id"] = "esp32-lora-gw-" + haTopic.field;
  payload["name"] = haTopic.name;
  payload["icon"] = "mdi:" + haTopic.icon;
  payload["unit_of_measurement"] = haTopic.unit;
  payload["state_topic"] = mainTopic + "/state";
  payload["value_template"] = "{{ value_json." + haTopic.field + " }}";

  if (haTopic.deviceClass != "")
  {
    payload["device_class"] = haTopic.deviceClass;
  }

  if (haTopic.stateClass != "")
  {
    payload["state_class"] = haTopic.stateClass;
  }

  // For later use, need to set all atributes for every sensor/topic
  // payload["entity_category"] = haTopic.entityCategory;

  String payloadSerialized;
  serializeJson(payload, payloadSerialized);

  if (client.connected())
  {
    client.publish(String(configTopic).c_str(), String(payloadSerialized).c_str(), true);
  }
}

void MQTTHomeAssistantDiscovery()
{
  // diagnostic information
  struct homeAssistantTopic uptime = {"uptime", "Uptime", "clock-time-eight-outline", "s", "", "", "diagnostic"};
  struct homeAssistantTopic MAC = {"mac", "MAC-Address", "network-outline", "", "", "", "diagnostic"};
  struct homeAssistantTopic hostname = {"hostname", "Hostname", "network-outline", "", "", "", "diagnostic"};
  struct homeAssistantTopic wifiRSSI = {"wifiRSSI", "WiFi-RSSI", "wifi", "dBm", "signal_strength", "", "diagnostic"};
  struct homeAssistantTopic loraRSSI = {"loraRSSI", "LoRa-RSSI", "wifi", "dBm", "signal_strength", "", "diagnostic"};
  struct homeAssistantTopic ip = {"ip", "IP", "network-outline", "", "", "", "diagnostic"};

  sendHomeAssistantDiscovery(uptime);
  sendHomeAssistantDiscovery(MAC);
  sendHomeAssistantDiscovery(hostname);
  sendHomeAssistantDiscovery(wifiRSSI);
  sendHomeAssistantDiscovery(loraRSSI);
  sendHomeAssistantDiscovery(ip);

  // sensor information
  struct homeAssistantTopic currentMeterValue = {"current-meter", "Water Consumption", "gauge", "m^3", "", "", ""};
  struct homeAssistantTopic preMeterValue = {"pre-meter", "Previous Water Consumption", "gauge", "m^3", "", "", ""};
  struct homeAssistantTopic temperature = {"temperature", "Temperature Forest", "thermometer", "°C", "", "", ""};
  struct homeAssistantTopic humidity = {"humidity", "Humidity Forest", "water-percent", "%", "", "", ""};
  struct homeAssistantTopic message = {"message", "LoRa Message", "message", "", "", "", ""};
  struct homeAssistantTopic packet_number = {"packet_number", "LoRa Packet Number", "counter", "", "", "", ""};

  sendHomeAssistantDiscovery(currentMeterValue);
  sendHomeAssistantDiscovery(preMeterValue);
  sendHomeAssistantDiscovery(temperature);
  sendHomeAssistantDiscovery(humidity);
  sendHomeAssistantDiscovery(message);
  sendHomeAssistantDiscovery(packet_number);
}

void loop()
{
  // Send Device Information only every minute
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > one_minute_interval)
  {
    previousMillis = currentMillis;
    if (client.connected())
    {
      sendDeviceInformationMQTT();
    }
  }

  // try to parse packet
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    // received a packet
    Serial.print("Received packet '");

    // read packet
    while (LoRa.available())
    {
      String LoRaData = LoRa.readString();
      Serial.println(LoRaData);

      // Send LoRa RSSI
      const int capacityPayload = JSON_OBJECT_SIZE(1);
      StaticJsonDocument<capacityPayload> payload;
      payload["loraRSSI"] = LoRa.packetRssi();

      String payloadSerialized;
      serializeJson(payload, payloadSerialized);

      if (client.connected())
      {
        client.publish(mqttState, String(LoRaData).c_str(), true);
        client.publish(mqttState, String(payloadSerialized).c_str(), true);
      }
    }
  }

  if (!client.connected())
  {
    reconnect();
  }
}

void reconnect()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    setupWiFi();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    setupMQTT();
  }
}

void sendDeviceInformationMQTT()
{
  const int capacityPayload = JSON_OBJECT_SIZE(14);
  StaticJsonDocument<capacityPayload> payload;

  payload["uptime"] = millis();
  payload["mac"] = WiFi.macAddress();
  payload["hostname"] = WiFi.getHostname();
  payload["wifiRSSI"] = WiFi.RSSI();
  payload["ip"] = WiFi.localIP();

  String payloadSerialized;
  serializeJson(payload, payloadSerialized);

  client.publish(mqttState, String(payloadSerialized).c_str(), true);
}