/*
 *  This sketch is for making an ESP8266 post sensor readings from a DHT11 sensor to data.sparkfun.com with values for location
 *  temp in F, temp in C, humidity in %.  
 *  A bit of this was kludged in from https://learn.adafruit.com/esp8266-temperature-slash-humidity-webserver/code
 *  Specifically I used the Adafruit libraries for the ESP8266WiFi and DHT sensor
 */

#include <ESP8266WiFi.h>
#include "DHT.h"

// DHT 11 sensor
#define DHTPIN 5
#define DHTTYPE DHT11

// DHT sensor
DHT dht(DHTPIN, DHTTYPE, 15);

const char* ssid = "YourSSID";
const char* password = "Your Password";

const char* host = "data.sparkfun.com";
const String pubkey = "YourPublicKey";
const String privkey = "YourPrivateKey";

const String location = "MasterBedroom";

void setup() {
  // Init sensor
  dht.begin();
  
  Serial.begin(115200);
  delay(10);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

int value = 0;

void loop() {
  delay(60000);
  ++value;

  // Grab the current state of the sensor
  float humidity_data = dht.readHumidity();
  float tempF_data = dht.readTemperature(true);
  float tempC_data = dht.readTemperature();
  
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
