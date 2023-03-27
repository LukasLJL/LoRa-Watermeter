# ESP32-LoRa-Gateway
This is the gateway part which receives lora signals and transforms them into mqtt messages for your internal iot network, like home assistant.

## Setup
To get started, you will need to adjust the secrets for your esp32.
For this you can rename the file `/src/secrets_example.h` to `/src/secrets.h` and adjust the parameters for your environment.

## Configuration
If you want to change the settings afterwards, you can connect to the same WiFi Network where the ESP32 is connected and use the web interface which is reachable at the ip-address of the esp32.
