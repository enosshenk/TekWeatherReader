// Tekventure Weather Sensor Reader
// Arduino Mega w/ Arduino Ethernet Shield
// Reads Sparkfun weather sensor mast and BME280 sensor

// Includes
#include <Wire.h>
#include <Ethernet.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 BME;

// Pins
const byte WSpeedPin = 3;
const byte WDirPin = A0;
const byte RainPin = 2;

// Global Variables
int LoopDelay = 1000;
int LastMillis = 0;
int LoopTime = 0;

float Humidity = 0.0;     // Humidity in percent
float TempF = 0.0;        // Temp in Farenheit
float TempC = 0.0;        // Temp in Celsius
float Pressure = 0.0;     // Temp in Pascals
float Dewpoint = 0.0;     // Dewpoint in Farenheit


void setup() {
  Serial.begin(9600);
 
  // Start BME280
  BME.begin();
}

void loop() {

  int Delta = millis() - LastMillis;
  LoopTime += Delta;
  LastMillis = millis();
  if (LoopTime > LoopDelay) {
    // Less than delay time, execute loop functions
    LoopTime = 0;
    UpdateBME();
  }

}

void UpdateBME() {
  // Polls BME sensor and sets values
  Humidity = BME.readHumidity();
  Pressure = BME.readPressure();
  TempC = BME.readTemperature();
  TempF = (TempC * 1.8) + 32;

  float DewPointC = 0;
  if (TempC != 0 && Humidity != 0)
  {
    // Calculate dew point
    float k;
    k = log(Humidity / 100) + (17.62 * TempC) / (243 + TempC);
    DewPointC = 243.12 * k / (17.62 - k);
    Dewpoint = (DewPointC * 1.8) + 32;
  } 
  Serial.print("TempF ");
  Serial.print(TempF);
  Serial.print("Humidity ");
  Serial.print(Humidity);
  Serial.print("Pressure ");
  Serial.print(Pressure);
  Serial.print("Dewpoint ");
  Serial.println(Dewpoint);
}
