# Demo

This setup contains a basic demo for the project, therein included is mosquitto as a mqtt-broker and home assistant to display and monitor the collected data.

## Setup
1. You will have to add a `passwd` file for mosquitto, run `mosquitto_passwd -b -c ./mqtt/config/passwd myUser myPassword` to create a new file.
2. If you want to add another user use the same command without the `-c`.
3. Run `docker compose up -d`.