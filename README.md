# Smart IOT Test
example program for demonstrating the API to access the following services:
- MQTT secured messaging connection to Azure IoT hub
- GNSS location 
- Log file on UFS flash on BG96 modem

# Description of operation
The program will start and connect to Azure IoT hub. It will then expect the hub to publish a system to device message with a payload OK (to be sent by server when it receives a message from the device with a payload 'HELLO').

This will start the GPS tracking loop. The application will regularly get the GNSS location, log the location or log an error if the location cannot be obtained. It will then initiate a connection to the Azure IoT hub to report the location using a json message. Example:
{"type":"STATUS","gnss":{"altitude":"-29.0","latitude":"47.215","longitude":"-1.564"},"utctime":"Mon, Aug 12, 2019 19:45"}

When the last connect time goes beyond a certain time, the application will also connect to receive a message from the system. It will connect to the server and send a message with a payload 'HELLO'. It will then listen to incoming messages from the server. When it receives a message or after the time window for reception is ellapsed, it will stop listening and issue a message 'BYE'.

The types of messages this particular application understands are special json messages formatted like this (note that quotes need to be replaced by # characters before sending to the device and with no space allowed):
```json
{#Type#:#CONFIG#,#GNSS_PERIOD#:60}
{#Type#:#CONFIG#,#CONNECT_PERIOD#:360}
{#Type#:#CONFIG#,#GNSS_PERIOD#:360,#CONNECT_PERIOD#:3600}
```

# Configuration

## IoT Hub related settings
The IoT Hub server domain can be configured in the file [_MQTT_server_setting.h_](./MQTT_server_setting.h). 
The device ID and Secret Key can be configured in the file [_MQTTClient_Settings.h_](./MQTTClient_Settings.h).

## APN/username/password
Depending on the toolchain, the APN/username/passord associated to the SIM card can be configured either using the [_mbed_app.json_](./mbed_app.json) file or directly defining the following macro constants:
- DEFAULT_APN
- APN_USERNAME
- APN_PASSWORD

## Debug console messages
Depending on the toolchain used BG96 debug can be turned on or off either using the [_mbed_app.json_](./mbed_app.json) file or setting the macro MBED_CONF_BG96_LIBRARY_BG96_DEBUG_SETTING to either 0x00 (No BG96 debug) or 0x84 (Full AT cmd display). 

You can turn the console output by setting the macro constant definition DEVICE_STDIO_MESSAGES to false.

You can also turn every debug services (ASSERT and so on) by defining the variable NDEBUG (add the -DNDEBUG flag to compiler when building the RELEASE version or your software).
