#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include "DHT.h"

// define the pins used by the lora module
#define ss 18
#define rst 14
#define dio0 26
#define DHTPIN 13

#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

int counter = 0;

void setup()
{
  // initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial)    ;
  dht.begin(); 
  Serial.println("LoRa Sender");

  // setup LoRa transceiver module
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(ss, rst, dio0);

  // replace the LoRa.begin(---E-) argument with your location's frequency
  // 433E6 for Asia
  // 866E6 for Europe
  // 915E6 for North America
  while (!LoRa.begin(866E6))
  {
    Serial.println(".");
    delay(500);
  }
  // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
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
