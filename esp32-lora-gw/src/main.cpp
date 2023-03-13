#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
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

// Functions
void setupLoRa();
void setupWiFi();
void setupMQTT();
void reconnect();
void sendHomeAssistantDiscovery();
void MQTTHomeAssistantDiscovery();

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

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

  // Setup Pin Configuration
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  setupLoRa();
  setupWiFi();
  setupMQTT();

  MQTTHomeAssistantDiscovery();
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
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
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

void setupMQTT()
{
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setBufferSize(1024);
  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    if (client.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASSWORD))
    {
      Serial.println("MQTT connected with name: " MQTT_CLIENT);
    }
    else
    {
      Serial.println("MQTT connection failed.");
    }
  }
  client.publish(mqttStatus, "connected", true);
}

void sendHomeAssistantDiscovery(homeAssistantTopic haTopic)
{
  const int capacityPayload = JSON_OBJECT_SIZE(26);
  StaticJsonDocument<capacityPayload> payload;

  String mainTopic = "esp-32-lora-gw";
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

  // For later use, need to set all atributes for every sensor/topic
  // payload["device_class"] = haTopic.deviceClass;
  // payload["state_class"] = haTopic.stateClass;
  // payload["entity_category"] = haTopic.entityCategory;

  String payloadSerialized;
  serializeJson(payload, payloadSerialized);

  client.publish(String(configTopic).c_str(), String(payloadSerialized).c_str(), true);
}

void MQTTHomeAssistantDiscovery()
{
  // diagnostic information
  struct homeAssistantTopic uptime = {"uptime", "Uptime", "clock-time-eight-outline", "s", "", "", "diagnostic"};
  struct homeAssistantTopic MAC = {"mac", "MAC-Address", "network-outline", "", "", "", "diagnostic"};
  struct homeAssistantTopic hostname = {"hostname", "Hostname", "network-outline", "", "", "", "diagnostic"};
  struct homeAssistantTopic wifiRSSI = {"wifi-rssi", "WiFi-RSSI", "wifi", "dBm", "signal_strength", "", "diagnostic"};
  struct homeAssistantTopic loraRSSI = {"lora-rssi", "LoRa-RSSI", "wifi", "dBm", "signal_strength", "", "diagnostic"};
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

  sendHomeAssistantDiscovery(currentMeterValue);
  sendHomeAssistantDiscovery(preMeterValue);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
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
      Serial.print(LoRaData);

      client.publish(mqttSensor, String(LoRaData).c_str(), true);
      client.publish(mqttSensor, String(LoRa.packetRssi()).c_str(), true);
    }
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
