

// Tekventure Weather Sensor Reader
// Arduino Mega w/ Arduino Ethernet Shield
// Reads Sparkfun weather sensor mast and BME280 sensor

// Includes
#include <SPI.h>
#include <Wire.h>
#include <Ethernet.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <bme68x.h>
#include <bme68x_defs.h>
#include <arduino-timer.h>
#include <ArduinoHttpClient.h>
#include <SD.h>

#define SEALEVELPRESSURE_HPA (1023.03)

Adafruit_BME680 BME;                          // BME680 object
auto SecondTimer = timer_create_default();    // Timer objects
auto MinuteTimer = timer_create_default();
auto DayTimer = timer_create_default();

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

float WindSpeedArray[60];       // Used to average values over a minute
float WindDirectionArray[60];

// URL for WUnderground
char UploadURL[] = "weatherstation.wunderground.com";

// MAC address and IP for the Ethernet Shield, uses static IP on router
byte mac[] = {
  0xAA, 0xBB, 0xCC, 0xDD, 0x23, 0x35
};
IPAddress ip(192, 168, 1, 23);

//EthernetServer server(80);      // Ethernet objects
EthernetClient EClient;
HttpClient Client = HttpClient(EClient, UploadURL, 80);   // HTTPclient library to avoid WUnderground errors

File LogFile;

void setup() {
  Serial.begin(9600);

  // Set up interrupt pins
  pinMode(WSpeedPin, INPUT_PULLUP);
  pinMode(RainPin, INPUT_PULLUP);  
 
  // Start BME280
  BME.begin();
  SPI.begin();
  BME.setTemperatureOversampling(BME680_OS_8X);
  BME.setHumidityOversampling(BME680_OS_2X);
  BME.setPressureOversampling(BME680_OS_4X);
  BME.setIIRFilterSize(BME680_FILTER_SIZE_3); 

  // Interrupts
  attachInterrupt(digitalPinToInterrupt(WSpeedPin), WindIRQ, FALLING);
  attachInterrupt(digitalPinToInterrupt(RainPin), RainIRQ, FALLING); 

  // Initialize timers
  SecondTimer.every(1000, SecondElapsed);
  MinuteTimer.every(60000, MinuteElapsed);
  DayTimer.every(86400000, DayElapsed);

  // Setup windspeed array
  for (int i=0; i<60; i++) 
  {
    WindSpeedArray[i] = -1.0;
    WindDirectionArray[i] = 0;
  }   
  
  Ethernet.init(10);
  // start the Ethernet connection
  Ethernet.begin(mac);
  
  // Init SD Card
  if (!SD.begin(4)) {
    Serial.println("SD Init fail");
  }
  
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
  
//  server.begin();
}

void loop() {

  // All we do in loop is tick the timers
  SecondTimer.tick();
  MinuteTimer.tick();
  DayTimer.tick();

}

void UpdateBME() {
  // Polls BME sensor and sets values
  
  if (! BME.performReading()) {
    Serial.println("Failed to perform BME reading :(");
    return;
  } 
  Humidity = BME.humidity;
  Pressure = BME.pressure;
  TempC = BME.temperature;
  TempF = (TempC * 1.8) + 32;       // Convert temp C to F

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
  // Interrupt from rain bucket dumping
  if (millis() - LastRainTime > 10)
  {
    LastRainTime = millis();
    RainTick++;
    RainMinute += 0.011;
    RainHour += 0.011;    
    RainDay += 0.011;
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

  // Hacky shit found from trial and error
  if (adc > 38 && adc < 42) return (90);
  if (adc > 62 && adc < 70) return (135);
  if (adc > 94 && adc < 100) return (180);
  if (adc > 164 && adc < 170) return (45);
  if (adc > 270 && adc < 275) return (225);
  if (adc > 427 && adc < 430) return (0);
  if (adc > 595 && adc < 599) return (315);
  if (adc > 735 && adc < 742) return (270);
  
  return LastWindDirection; // If all else fails send last second
}

bool SecondElapsed(void *) {
  // Executed every second

  // Update BME values
  UpdateBME();  

  // Check if this second's wind speed is greater than the gust value
  WindSpeedArray[Second] = GetWindspeed(); 
  if (WindSpeedArray[Second] > WindGust) {
    WindGust = WindSpeedArray[Second];
  }
  WindDirectionArray[Second] = GetWindDirection();
  LastWindDirection = WindDirectionArray[Second];

  // Increment seconds variable probably not needed anymore
  Second++;   
  return true;
}

bool MinuteElapsed(void *) {
  // Executes every 60 seconds
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
  Serial.print("Wind Gust ");
  Serial.println(WindGust);
  Serial.print("WindDirection ");
  Serial.println(WindDirection);

  Serial.print("Rain Minute ");
  Serial.println(RainMinute); */

  // Every minute we call upload
  UploadData();

  // Reset gust value
  WindGust = 0.0;

  // Reset rain counts for each hour
  if (Minute == 60) {
    // One hour has passed
    RainHour = 0.0;
    Minute = 0;
  }  
  return true;
}

bool DayElapsed(void *) {
  // 24hr elapsed, clear rain accum
  RainMinute = 0.0;
  RainHour = 0.0;
  RainDay = 0.0;
  return true;
}

void UploadData() {
  // Sends data to Weather Underground
  
  // Convert pressure from Pa to InMG
  float OutPressure = Pressure * 0.00029530; 
  
  LogFile = SD.open("log.txt", FILE_WRITE);
  
  // Build up GET request
  String Request = "/weatherstation/updateweatherstation.php?ID=KINFORTW318&PASSWORD=Tfw37q2V&dateutc=now"
  + String("&winddir=") + String(WindDirection)
  + String("&windspeedmph=") + String(WindSpeed)
  + String("&windgustmph=") + String(WindGust)
  + String("&rainin=") + String(RainHour)
  + String("&dailyrainin=") + String(RainDay)
  + String("&baromin=") + String(OutPressure)
  + String("&humidity=") + String(Humidity)
  + String("&tempf=") + String(TempF)
  + String("&dewptf=") + String(Dewpoint)
  + String("&action=updateraw");
  

  Serial.println(Request);
  if (LogFile) {
    String LogData = millis() + " - " + Request;
    LogFile.println(LogData);
  }
  
  // Send request
  Client.get(Request);
  
  // read the status code and body of the response
  int statusCode = Client.responseStatusCode();
  String response = Client.responseBody();
  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response); 
  
  if (LogFile) {
    String LogData = millis() + " - Status Code: " + (String)statusCode + " - Response: " + (String)response;
    LogFile.println(LogData);
    LogFile.close();
  }
}

/* void listenForEthernetClients() {
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
} */
