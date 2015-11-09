/*
 *  This sketch is for making an ESP8266 display the local weather forecast and room temperature/humitiy on an OLED.
 *  It will also post sensor readings from a DHT11 sensor to data.sparkfun.com with values for location
 *  temp in F, temp in C, humidity in %.  The core of this project is https://learn.adafruit.com/huzzah-weather-display
 *  but like many things I hacked on it a bit to fit my needs.  I didn't like how they left a bunch of headers sticking
 *  out the side of the unit for users to stab themselves on. I also changed up the wiring a bit, designed a 3D printed
 *  case for everything, and an FTDI to Female USB A adaptor. Check out the files on my Github page. 
 *  https://github.com/kmcgerald/floppy-waffle/tree/master/HomeClimate
 *  A bit of this was kludged in from https://learn.adafruit.com/esp8266-temperature-slash-humidity-webserver/code
 *  Specifically I used the Adafruit libraries for the ESP8266WiFi and DHT sensor
 *  I also use the RunningMedian library from http://playground.arduino.cc/Main/RunningMedian to cope will random sensor spikes.
 */
 
 /**The MIT License (MIT)

Copyright (c) 2015 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/

#include <Wire.h>
#include <Ticker.h>
#include "ssd1306_i2c.h"
#include "icons.h"


#include <ESP8266WiFi.h>
#include "WeatherClient.h"

#include "RunningMedian.h"

// These are required for reading from the DHT11 sensor 
#include "DHT.h"
#define DHTPIN 5
#define DHTTYPE DHT11

#define SDA 14
#define SCL 12
//#define RST 2

#define I2C 0x3D

#define WIFISSID "YourSSID"
#define PASSWORD "Your Password"

#define FORECASTAPIKEY "YourForecastIOKey"

// Home
#define LATITUDE 0.024
#define LONGITUDE -1.521

//DHT sensor
DHT dht(DHTPIN, DHTTYPE, 15);

const char* host = "data.sparkfun.com";
const String pubkey = "YourPublicKey";
const String privkey = "YourPrivateKey";

const String location = "MasterBedroom";

// Initialize the oled display for address 0x3c
// 0x3D is the adafruit address....
// sda-pin=14 and sdc-pin=12
SSD1306 display(I2C, SDA, SCL);
WeatherClient weather;
// We need 3 Tickers to keep track of when to update things
Ticker weatherTicker;
Ticker uploadTicker;
Ticker readingTicker;

// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
void (*frameCallbacks[4])(int x, int y) = {drawFrame1, drawFrame2, drawFrame3, drawFrame4};

// how many frames are there?
int frameCount = 4;
// on frame is currently displayed
int currentFrame = 0;

// your network SSID (name)
char ssid[] = WIFISSID;

// your network password
char pass[] = PASSWORD;

// Go to forecast.io and register for an API KEY
String forecastApiKey = FORECASTAPIKEY;

// Coordinates of the place you want
// weather information for
double latitude = LATITUDE;
double longitude = LONGITUDE;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = true;

// flag changed in the ticker function every 60 seconds
bool readyForSensorUpload = false;
bool readyForSensorReading = true;
float humidity_data;
float tempF_data;
float tempC_data;
//The DHT11 sensor seems subject to spikes of a few degrees over actual so I'm going to take readings between the upload intervals and upload the median
RunningMedian humSamples = RunningMedian(10);
RunningMedian tempCSamples = RunningMedian(10);
RunningMedian tempFSamples = RunningMedian(10);
  
void setup() {
  delay(500);
  //ESP.wdtDisable();

  dht.begin();
  
  // initialize display
  display.init();
  display.flipScreenVertically();
  // set the drawing functions
  display.setFrameCallbacks(4, frameCallbacks);
  // how many ticks does a slide of frame take?
  display.setFrameTransitionTicks(10);

  display.clear();
  display.display();

  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println();
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    display.clear();
    display.drawXbm(34, 10, 60, 36, WiFi_Logo_bits);
    display.setColor(INVERSE);
    display.fillRect(10, 10, 108, 44);
    display.setColor(WHITE);
    drawSpinner(4, counter % 4);
    display.display();

    counter++;
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // update the weather information every 10 mintues only
  // forecast.io only allows 1000 calls per day
  weatherTicker.attach(60 * 10, setReadyForWeatherUpdate);

  // upload the temp and humidity sensor values every 60 seconds 
  uploadTicker.attach(60, setReadyForSensorUpload);

  // udpate the running array of sensor values every 6 seconds
  readingTicker.attach(6, setReadyForSensorReading);
  
  //ESP.wdtEnable();
}

void loop() {

  if (readyForWeatherUpdate && display.getFrameState() == display.FRAME_STATE_FIX) {
    readyForWeatherUpdate = false;
    weather.updateWeatherData(forecastApiKey, latitude, longitude);
  }

  display.clear();
  display.nextFrameTick();
  display.display();

  if (readyForSensorReading && display.getFrameState() == display.FRAME_STATE_FIX) {
    readyForSensorReading = false;
    Serial.println("Reading DHT Sensor.");
    // Grab the current state of the sensor
    float reading = dht.readHumidity();
    Serial.print("Humidity: " + String(reading));
    humSamples.add(reading);
    reading = dht.readTemperature();
    Serial.print(" Temp C: " + String(reading));
    tempCSamples.add(reading);
    reading = dht.readTemperature(true);
    Serial.println(" Temp F: " + String(reading));
    tempFSamples.add(reading);
    //Serial.println("Bools: " + String(readyForWeatherUpdate) + String(readyForSensorUpload) + String(readyForSensorReading));  
  }

  if (readyForSensorUpload && display.getFrameState() == display.FRAME_STATE_FIX) {
    readyForSensorUpload = false;
    Serial.println("Uploading Sensor Median.");
    uploadDHTSensor();
    humSamples.clear();
    tempCSamples.clear();
    tempFSamples.clear();
  }
}

void setReadyForWeatherUpdate() {
  readyForWeatherUpdate = true;
}

void setReadyForSensorUpload() {
  readyForSensorUpload = true;
}

void setReadyForSensorReading() {
  readyForSensorReading = true;
}

void drawFrame1(int x, int y) {
  display.setFontScale2x2(false);
  display.drawString(65 + x, 8 + y, "Now");
  display.drawXbm(x + 7, y + 7, 50, 50, getIconFromString(weather.getCurrentIcon()));
  display.setFontScale2x2(true);
  display.drawString(64 + x, 20 + y, String(weather.getCurrentTemp()) + "F");

  //display.setFontScale2x2(false);
  //display.drawString(50 + x, 40 + y, String(weather.getSummaryToday()));

}

const char* getIconFromString(String icon) {
  //"clear-day, clear-night, rain, snow, sleet, wind, fog, cloudy, partly-cloudy-day, or partly-cloudy-night"
  if (icon == "clear-day") {
    return clear_day_bits;
  } else if (icon == "clear-night") {
    return clear_night_bits;
  } else if (icon == "rain") {
    return rain_bits;
  } else if (icon == "snow") {
    return snow_bits;
  } else if (icon == "sleet") {
    return sleet_bits;
  } else if (icon == "wind") {
    return wind_bits;
  } else if (icon == "fog") {
    return fog_bits;
  } else if (icon == "cloudy") {
    return cloudy_bits;
  } else if (icon == "partly-cloudy-day") {
    return partly_cloudy_day_bits;
  } else if (icon == "partly-cloudy-night") {
    return partly_cloudy_night_bits;
  }
  return cloudy_bits;
}

void drawFrame2(int x, int y) {
  display.setFontScale2x2(false);
  display.drawString(65 + x, 0 + y, "Today");
  display.drawXbm(x, y, 60, 60, xbmtemp);
  display.setFontScale2x2(true);
  display.drawString(64 + x, 14 + y, String(weather.getCurrentTemp()) + "F");
  display.setFontScale2x2(false);
  display.drawString(66 + x, 40 + y, String(weather.getMinTempToday()) + "F/" + String(weather.getMaxTempToday()) + "F");

}

void drawFrame3(int x, int y) {
  display.drawXbm(x + 7, y + 7, 50, 50, getIconFromString(weather.getIconTomorrow()));
  display.setFontScale2x2(false);
  display.drawString(65 + x, 7 + y, "Tomorrow");
  display.setFontScale2x2(true);
  display.drawString(64 + x, 20 + y, String(weather.getMaxTempTomorrow()) + "F");
}

void drawFrame4(int x, int y) {
  display.setFontScale2x2(false);
  display.drawString(65 + x, 0 + y, "Room");
  display.drawXbm(x, y, 60, 60, xbmtemp);
  display.setFontScale2x2(true);
  display.drawString(64 + x, 14 + y, String(dht.readTemperature(true)) + "F");
  display.setFontScale2x2(false);
  display.drawString(66 + x, 40 + y, String(dht.readHumidity()) + "%");

}

void drawSpinner(int count, int active) {
  for (int i = 0; i < count; i++) {
    const char *xbm;
    if (active == i) {
      xbm = active_bits;
    } else {
      xbm = inactive_bits;
    }
    display.drawXbm(64 - (12 * count / 2) + 12 * i, 56, 8, 8, xbm);
  }
}

void uploadDHTSensor() {
  humidity_data = humSamples.getMedian();
  tempF_data = tempFSamples.getMedian();
  tempC_data = tempCSamples.getMedian();
  
  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // We now create a URI for the request
  String url = "/input/" + pubkey + "?private_key=" + privkey + "&location=" + location + "&humidity=" + String(humidity_data) + "&tempf=" + String(tempF_data) + "&tempc=" + String(tempC_data);
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  delay(10);
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  
  Serial.println();
  Serial.println("closing connection");
}

