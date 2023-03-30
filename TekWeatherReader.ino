// Tekventure Weather Sensor Reader
// Arduino Mega w/ Arduino Ethernet Shield
// Reads Sparkfun weather sensor mast and BME280 sensor

// Includes
#include <Wire.h>
#include <Ethernet.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <arduino-timer.h>


Adafruit_BME280 BME;
auto SecondTimer = timer_create_default();
auto MinuteTimer = timer_create_default();

// Pins
const byte WSpeedPin = 3;
const byte WDirPin = A0;
const byte RainPin = 2;

// Global Variables
int LastWindTime = 0;
int LastRainTime = 0;
int WindTick = 0;
int RainTick = 0;
long Second = 0;
long Minute = 0;
long LastWind = 0;
float RainMinute = 0.0;
float RainHour = 0.0;
float RainDay = 0.0;

float Humidity = 0.0;     // Humidity in percent
float TempF = 0.0;        // Temp in Farenheit
float TempC = 0.0;        // Temp in Celsius
float Pressure = 0.0;     // Temp in Pascals
float Dewpoint = 0.0;     // Dewpoint in Farenheit
float WindSpeed = 0.0;    // Wind speed MPH
float WindSpeedArray[60];


void setup() {
  Serial.begin(9600);

  pinMode(WSpeedPin, INPUT_PULLUP);
  pinMode(RainPin, INPUT_PULLUP);  
 
  // Start BME280
  BME.begin();

  // Interrupts
  attachInterrupt(digitalPinToInterrupt(WSpeedPin), WindIRQ, FALLING);
  attachInterrupt(digitalPinToInterrupt(RainPin), RainIRQ, FALLING); 

  SecondTimer.every(1000, SecondElapsed);
  MinuteTimer.every(60000, MinuteElapsed);
  
  for (int i=0; i<60; i++) 
  {
    // Setup windspeed array
    WindSpeedArray[i] = -1.0;
  }    
}

void loop() {

  SecondTimer.tick();
  MinuteTimer.tick();

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
}

void WindIRQ() {
  // Interrupt from the wind sensor ticking over
  if (millis() - LastWindTime > 10)
  {
    LastWindTime = millis();
    WindTick++;
  }
}

void RainIRQ() {
  if (millis() - LastRainTime > 10)
  {
    LastRainTime = millis();
    RainTick++;
    RainMinute += 0.011;
    RainHour += 0.11;    
  }
}

float GetWindspeed() {
  // Get current wind speed
  float Delta = millis() - LastWind;
  Delta /= 1000.0;  
  float CurrentWindSpeed = (float)WindTick / Delta;
  WindTick = 0;
  LastWind = millis();
  CurrentWindSpeed *= 1.492;

  return (CurrentWindSpeed);  
}

bool SecondElapsed(void *) {
  UpdateBME();
  
  WindSpeedArray[Second] = GetWindspeed();  
  Second++;   
  Serial.println("Tick!");
  return true;
}

bool MinuteElapsed(void *) {
  Minute++;
  Second =  0;
  
  // Average 60 seconds of wind events
  float WindSpeedTotal = 0;
  for (int i=0; i<60; i++) 
  {
    WindSpeedTotal += WindSpeedArray[i];
  }
  WindSpeed = WindSpeedTotal / 60;
  Serial.print("Windspeed ");
  Serial.println(WindSpeed);
  
  if (Minute == 60) {
    // One hour has passed
    RainHour = 0.0;
    Minute = 0;
  }
  Serial.print("Rain Minute ");
  Serial.println(RainMinute);
  
  return true;
}
