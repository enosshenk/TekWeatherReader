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
int WindDirection = 0;
int LastWindDirection = 0;
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
float WindGust = 0.0;

float WindSpeedArray[60];
float WindDirectionArray[60];

char UploadURL[] = "weatherstation.wunderground.com";

byte UploadIP[] = {
  52, 22, 134, 222
};
// MAC address and IP for the Ethernet Shield, uses static IP on router
byte mac[] = {
  0xAA, 0xBB, 0xCC, 0xDD, 0x23, 0x35
};
IPAddress ip(192, 168, 1, 23);

EthernetServer server(80);
EthernetClient Client;


void setup() {
  Serial.begin(9600);

  pinMode(WSpeedPin, INPUT_PULLUP);
  pinMode(RainPin, INPUT_PULLUP);  
 
  // Start BME280
  BME.begin();
  SPI.begin();

  // Interrupts
  attachInterrupt(digitalPinToInterrupt(WSpeedPin), WindIRQ, FALLING);
  attachInterrupt(digitalPinToInterrupt(RainPin), RainIRQ, FALLING); 

  SecondTimer.every(1000, SecondElapsed);
  MinuteTimer.every(60000, MinuteElapsed);
  
  for (int i=0; i<60; i++) 
  {
    // Setup windspeed array
    WindSpeedArray[i] = -1.0;
    WindDirectionArray[i] = 0;
  }   
  
  Ethernet.init(10);
  // start the Ethernet connection
  Ethernet.begin(mac, ip);
  
  delay(2000);
  
     // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield not found");
    while (true) {
      delay(1);
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }  
  
  server.begin();
}

void loop() {

  SecondTimer.tick();
  MinuteTimer.tick();
  
  if (Client.available()) {
    char c = Client.read();
    Serial.print(c);
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

int GetWindDirection() {
  unsigned int adc;

  adc = analogRead(WDirPin);
 // Serial.println(adc);

  if (adc > 38 && adc < 42) return (90);
  if (adc > 62 && adc < 70) return (135);
  if (adc > 94 && adc < 100) return (180);
  if (adc > 164 && adc < 170) return (45);
  if (adc > 270 && adc < 275) return (225);
  if (adc > 427 && adc < 430) return (0);
  if (adc > 595 && adc < 599) return (315);
  if (adc > 735 && adc < 742) return (270);
  
  return LastWindDirection; // error, disconnected?  
}

bool SecondElapsed(void *) {
  UpdateBME();  
  WindSpeedArray[Second] = GetWindspeed(); 
  if (WindSpeedArray[Second] > WindGust) {
    WindGust = WindSpeedArray[Second];
  }
  WindDirectionArray[Second] = GetWindDirection();
  LastWindDirection = WindDirectionArray[Second];
  Second++;   
  return true;
}

bool MinuteElapsed(void *) {
  Minute++;
  Second =  0;
  
  // Average 60 seconds of wind events and directions
  float WindSpeedTotal = 0;
  float WindDirectionTotal = 0;
  for (int i=0; i<60; i++) 
  {
    WindSpeedTotal += WindSpeedArray[i];
    WindDirectionTotal += WindDirectionArray[i];
  }
  WindSpeed = WindSpeedTotal / 60;
  if (WindSpeed < 0) { WindSpeed = 0; }
  WindDirection = WindDirectionTotal / 60;
 /* Serial.print("Windspeed ");
  Serial.println(WindSpeed);
  Serial.print("WindDirection ");
  Serial.println(WindDirection);  */
  
  WindGust = 0.0;
  
  if (Minute == 60) {
    // One hour has passed
    RainHour = 0.0;
    Minute = 0;
  }
 /* Serial.print("Rain Minute ");
  Serial.println(RainMinute); */
  
  UploadData();
  
  return true;
}

void UploadData() {
  // Format URL to uploadd to wunderground
  if ( Client.connect(UploadURL, 80) )
  {
    Serial.println("Connected to WUnderground");

    Client.print("GET /weatherstation/updateweatherstation.php?ID=KINFORTW318&PASSWORD=Tfw37q2V&dateutc=now");
  //  Client.print("&winddir=");
  //  Client.print(WindDirection);
 //   Client.print("&windspeed=");
 //   Client.print((int)WindSpeed);
 //   Client.print("&windgust=");
 //   Client.print((int)WindGust);
 //   Client.print("&humidity=");
 //   Client.print((int)Humidity);
 //   Client.print("&baromin=");
 //   int OutPressure = Pressure * 0.00029530;
 //   Client.print((int)OutPressure);
 //   Client.print("&tempf=");
 //   Client.print((int)TempF);
 //   Client.print("&rainhour=");
 //   Client.print((int)RainHour);
 //  Client.print("&dewptf=");
 //   Client.print((int)Dewpoint);
 //   Client.print("&action=updateraw");      
    Client.print("/ HTTP/1.1\r\nHost\: http://weatherstation.wunderground.com \r\nConnection\: close\r\n\r\n");
    Client.print("\r\n\r\n\r\n");
    Client.println();
    Client.println();

  } else { Serial.println("Connection failed"); }
}

void listenForEthernetClients() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("Got a client");
    // an http request ends with a blank line
    bool currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          client.print("<h1>FNORD</h1>");       
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }
}
