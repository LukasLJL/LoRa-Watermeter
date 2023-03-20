#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include "DHT.h"

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

// DHT
DHT dht(DHTPIN, DHTTYPE);

int counter = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("LoRa Sender");

  dht.begin();
  
  setupLoRa();
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

void loop()
{
  StaticJsonDocument<96> payload;

  payload["humidity"] = dht.readHumidity();
  payload["temperature"] = dht.readTemperature();
  payload["packet_number"] = counter;
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
