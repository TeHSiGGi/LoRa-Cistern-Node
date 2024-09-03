# LoRa-Cistern-Node

A LoRa node to measure the level of a rainwater cistern via ultrasonic. It also includes monitoring of the cistern temperature and battery level.

The node is powered via AA batteries and uses several techniques to reduce power consumption.

## Hardware

At the heart of the Node is an arduino pro mini, running at 1MHz to reduce power consumption. Programming is performed via ISP instead of serial programming.

The Pro Mini has its onboard regulator and power LED removed, to further save energy.
The Node is powered by 4 AA batteries, which are resulting in a base voltage of 6V when full. The Arduino itself is powered via a MCP1700 with 3.3V output. Since the ATmega on the 328P is consuming very low power, the linear regulator is good enough.

For the sensors as well as for the LoRa Transceiver, a separate regulator is being used. It is a switching regulator ISL85415 with a shutdown pin, allowing to turn off the power rail completely when not in use. This will power the temperature sensor, the ultrasonic sensor and the LoRa module.

The ATmega will also measure the battery voltage over a high resistance resistor divider (6.6MOhm / 1MOhm) with the internal reference voltage of 1.1V.

The ultrasonic sensor is an A02YYUW - talked to via UART using 9600 baud. The sensors mode select pin is set to LOW, allowing for fast distance readings.

The temperature sensor is a DS18B20 talked to via OneWire bus.

## Software

### LoRa Node

The LoRa Node runs a simple arduino based program that will use the sleep function of the LowPower library to keep the ATmega sleeping for around 10 minutes. Every four sleep cycles of 8 seconds the program will blink the onboard LED for 1ms as low power heartbeat.

Every 75 cycles (~10 minutes) the program will be performing the send data routine.

It will send the sensor data in a very specific format, but not processed. This will ensure we don't waste any processing time in the Node.

The message that is being transmitted will look like this:

| byte 0 | byte 1 | byte 2 | byte 3+ |
| ------ | ------ | ------ | ------- |
| Receiver Address Byte | Sender Address Byte | Message length Byte | Message data |

The message is structured the following way:

`T:3096:V:707:D:241`

T - Temperature sensor raw value
V - ADC reading raw value
D - Distance value in mm

### Raspberry Pi

On the Raspberry Pi, the SX1278 transceiver module is set to continuous receive mode and will be accessed via SPI using the pyLoRa library.
The script exposes a web server on port 5000 using flask and python. 

The data is available via JSON on `/api/latest-data` - There is also a simple HTML via on `/`.

If desired, the data can also be sent via MQTT.

Use the `setup.sh` script to `install` the python dependencies, and setup the `systemd` service.

Clone the code repository to `/home/pi`.