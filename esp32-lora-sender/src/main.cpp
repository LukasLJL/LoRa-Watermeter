#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebConfig.h>
#include <Ticker.h>
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
void setupTimer();
String getWatermeterIP();
void sendLoRa();
void handleRoot();

// DHT
DHT dht(DHTPIN, DHTTYPE);

// Variables
int counter = 0;
String watermeterIP = "127.0.0.1";

String params = "["
  "{"
  "'name':'ssid',"
  "'label':'Name of WiFi',"
  "'type':"+String(INPUTTEXT)+","
  "'default':'MyIoTWiFi'"
  "},"
  "{"
  "'name':'pwd',"
  "'label':'WiFi Password',"
  "'type':"+String(INPUTPASSWORD)+","
  "'default':'MyPassword'"
  "},"
  "{"
  "'name':'interval',"
  "'label':'LoRa Interval in seconds',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':1,'max':86400,"
  "'default':'60'"
  "}"
  "]";

WebServer server;
WebConfig conf;
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

  Serial.println(params);
  conf.setDescription(params);
  conf.readConfig();

  // Setup Pin Configuration
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  dht.begin();

  setupLoRa();
  setupWiFi();
  setupTimer();

  server.on("/",handleRoot);
  server.begin(80);
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



  if (conf.values[0] != "")
  {
    WiFi.softAP(conf.values[0].c_str(),conf.values[1].c_str());
    Serial.println("Started WiFi-AP with the SSID: " + conf.values[0]);
  } else
  {
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Started WiFi-AP with the SSID: " + String(WIFI_SSID));
  }

  

  Serial.print("AP-IP: ");
  Serial.println(WiFi.softAPIP());
}

void setupTimer()
{
  if (conf.values[2] != "")
  {
    timer.attach_ms(1000 * conf.getInt("interval"), sendLoRa); 
    Serial.println("Started LoRa Interval with " + conf.values[2]);
  } else
  {
    timer.attach_ms(10000, sendLoRa); 
    Serial.println("Started LoRa Interval with " + 1);
  }
}

void loop()
{
  server.handleClient();
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
   Serial.println("Send Water");
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

void handleRoot() {
  conf.handleFormRequest(&server);
  if (server.hasArg("SAVE")) {
    uint8_t cnt = conf.getCount();
    Serial.println("*********** Configuration ************");
    for (uint8_t i = 0; i<cnt; i++) {
      Serial.print(conf.getName(i));
      Serial.print(" = ");
      Serial.println(conf.values[i]);
    }
  }
}