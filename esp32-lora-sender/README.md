# ESP32-LoRa-Sender
This is the sender part which checks for an existing watermeter and reads the metrics additionally it uses a DHT22 to get the current temperature and humidity.
The collected data is sent via LoRa to the Gateway. 

## Setup
To get started, you will need to adjust the secrets for your esp32.
For this you can rename the file `/src/secrets_example.h` to `/src/secrets.h` and adjust the parameters for your environment.

## Configuration
If you want to change the settings afterwards, you can connect to the WiFi of the ESP32 and use the web interface which is reachable at [192.168.4.1](http://192.168.4.1).
