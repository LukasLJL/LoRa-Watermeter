#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
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

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

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
      abort();
    }
  }
  client.publish(mqttStatus, "connected", true);
}

void loop()
{
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
