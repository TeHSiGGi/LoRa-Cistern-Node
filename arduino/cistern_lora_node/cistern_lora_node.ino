#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <LoRa.h>
#include <LowPower.h>

// Lora Module PIN definition
#define LORA_SSN 10
#define LORA_REST 14
#define LORA_IRQ 2

// Power enable for the 3.3V buck regulator
#define POWER_ENABLE 17

// Debug LED
#define DEBUG_LED 13

// One wire DATA
#define ONE_WIRE_DATA 3

// Define LoRa addresses
#define NODE_ADDRESS 0xFA
#define RECEIVER_ADDRESS 0xB4

// One wire setup
OneWire oneWire(ONE_WIRE_DATA);
DallasTemperature sensors(&oneWire);

// Variable to store the sensor's address
// Storing it makes the temperature reading faster in the end, since we do not need to scan the bus every time
DeviceAddress tempSensorAddress;

// Timing counter for sleep logic
int timeCounter = 0;

void setup() {
  // Configure the ADC to use the internal reference
  analogReference(INTERNAL);
  
  // Configure and set the power pins, enable one wire for quick setup
  pinMode(POWER_ENABLE, OUTPUT);
  digitalWrite(POWER_ENABLE, 1);

  // Manual Lora Pin control
  pinMode(LORA_SSN, OUTPUT);
  pinMode(LORA_REST, OUTPUT);

  // DEBUG LED
  pinMode(DEBUG_LED, OUTPUT);
  digitalWrite(DEBUG_LED, 0);

  // We wait for a while, so the 3.3V bus can settle.
  // We signal this via the LED blinking very fast
  for (int i = 0; i < 5; i++) {
    digitalWrite(DEBUG_LED, 1);
    delay(100);
    digitalWrite(DEBUG_LED, 0);
    delay(400);
  }

  // Waiting done, let's initialize the OneWire bus
  sensors.begin();

  // Some retry logic to determine the address of the DS18B20 sensor
  // If we can not get the sensor, we will continue our execution later.
  int retry = 0;

  while(!sensors.getAddress(tempSensorAddress, 0)) {
    retry++;
    // Give a fitting blinking LED, that displays we're waiting for the sensor
    digitalWrite(DEBUG_LED, 1);
    delay(100);
    digitalWrite(DEBUG_LED, 0);
    delay(900);
    if (retry > 3) {
      break;
    }
  }

  // Once we found the address, we disable the 3.3V power again
  digitalWrite(POWER_ENABLE, 0);

  // Setup LoRa transceiver module
  LoRa.setPins(LORA_SSN, LORA_REST, LORA_IRQ);

  // Just make sure that the debug LED is off, better safe than sorry
  digitalWrite(DEBUG_LED, 0);
}


/*
 * Reads the temperature from one wire sensor
 * It will send the raw 12 bit data, since this will keep the active
 * time of the arduino as short as possible. The data is also very
 * small, making it faster to send
 */
int16_t readTemperature() {
  // Request temperature and get the actual 12 bit value
  sensors.requestTemperaturesByAddress(tempSensorAddress);
  int16_t result = sensors.getTemp(tempSensorAddress);
  return result;
}

int readDistance() {
  uint8_t data[4];

  // Wait for the start
  do {
    data[0] = Serial.read();
  } while (data[0] != 0xff);

  data[1] = Serial.read();
  data[2] = Serial.read();
  data[3] = Serial.read();
  
  // Clear the serial buffer
  Serial.flush();

  // Now that we've got the data, we'll calculate the result
  if (data[0] == 0xFF) {
    int checkSum;
    int distance;
    checkSum = (data[0] + data[1] + data[2]) & 0x00FF;
    if (checkSum == data[3]) {
      distance = (data[1] << 8) + data[2];
      // Check if the distance is within the range we support
      return distance;
    } else {
      // Number for checksum error
      return -888;
    }
  }

  // If we don't have that data.. ehh wtf?
  return -1;
}

/*
 * Build the message and send it
 * The telegram is formatted: "T:<tempvalue:D:<depthvalue>"
 * We will power on the 3.3V on demand to ensure minimal power usage
 */
void sendData() {
  Serial.begin(9600);
  // Turn on the 3.3V
  digitalWrite(POWER_ENABLE, 1);
  // wait for the bus to settle
  delay(50);
  // Initialize the SX1278 module
  while (!LoRa.begin(433E6)) {  // Frequency set to 433 MHz
    digitalWrite(DEBUG_LED, 1);
    delay(500);
    digitalWrite(DEBUG_LED, 0);
    delay(500);
  }
  // Build message
  String message = "";
  message += "T:";
  message += String(readTemperature());
  message += ":V:";
  message += String(analogRead(1));
  message += ":D:";
  message += String(readDistance());
  // Begin LoRa transmission
  LoRa.beginPacket();
  // Write addresses
  LoRa.write(RECEIVER_ADDRESS);
  LoRa.write(NODE_ADDRESS);
  // Add message length
  LoRa.write(message.length());
  // Add message itself
  LoRa.print(message);
  // End Lora Packet
  LoRa.endPacket();
  LoRa.end();
  // Power down the LoRa GPIOs explicitly
  digitalWrite(LORA_SSN, 0);
  digitalWrite(LORA_REST, 0);
  Serial.end();
  // Power off again
  digitalWrite(POWER_ENABLE, 0);
}

void loop() {
  timeCounter++;
  if (timeCounter % 4 == 0) {
    // We'll blink the LED every 4 cycles ~ 32s  
    digitalWrite(13, 1);
    delay(1);
    digitalWrite(13, 0);
  }
  // Send the data, if we've been waiting for 10 minutes
  // 10 minutes = 8s * 75 sleep cycles = 600s
  if (timeCounter >= 75) {
    sendData();
    timeCounter = 0;
  }
  // Let's go to sleep
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}
