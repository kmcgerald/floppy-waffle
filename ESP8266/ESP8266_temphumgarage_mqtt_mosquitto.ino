/* This sketch is my attempt to monitor my garage temp, humidity, and garage bay doors with an ESP8266 module.
 *  The module will monitor the door sensors and if any of them open (normally closed) it will send a specially formatted
 *  JSON message to my MQTT broker (mosquitto on a Beagle Bone Black) which will be relayed to the OwnTracks client on
 *  my Android phone.  Since the OwnTracks message format requires a UNIX epoch formatted timestamp, I get to roll in some
 *  NTP over UDP for fun.  I used a public domain example of NTP over UDP for ESP8266 by Ivan Grokhotkov
 */
 
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <WiFiUdp.h>  // Needed for NTP over UDP
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "RunningMedian.h"
#include <Ticker.h>

#define MQTT_VERSION MQTT_VERSION_3_1  //Override (lower) the version for older Mosquitto 1.2.2 on BBB

const int CarPin = 12;
const int TruckPin = 13;

// Sensor state trackers
int carDoorState = 0;
int truckDoorState = 0;

// DHT 11 sensor
#define DHTPIN 5
#define DHTTYPE DHT11

// DHT sensor
DHT dht(DHTPIN, DHTTYPE, 15);
//The DHT11 sensor seems subject to spikes of a few degrees over actual so I'm going to take readings between the upload intervals and upload the median
RunningMedian humSamples = RunningMedian(10);
RunningMedian tempCSamples = RunningMedian(10);
RunningMedian tempFSamples = RunningMedian(10);

// We need 2 Tickers to keep track of when to update things
Ticker uploadTicker;
Ticker readingTicker;

// flag changed in the ticker function every 60 seconds
bool readyForSensorUpload = false;
bool readyForSensorReading = true;
float humidity_data;
float tempF_data;
float tempC_data;

// Update these with values suitable for your network.
const char* ssid[] = {"MyWIFIAP","MyBackupAP"};
const char* password[] = {"The Secure Passphrase I should have","The Secure Passphrase for my backup AP"};
const int numssid = 2; // How many SSIDs in the array above

// Setting up UDP stuff for NTP
unsigned int localPort = 2390;      // local port to listen for UDP packets
/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

const char* mqtt_server = "192.168.1.175";

WiFiClient espClient;
PubSubClient client(espClient);
WiFiClient aioClient;
/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "my_aio_username"
#define AIO_KEY         "my_aio_key"
// Store the MQTT server, client ID, username, and password in flash memory.
// This is required for using the Adafruit MQTT library.
const char MQTT_SERVER[] PROGMEM    = AIO_SERVER;
// Set a unique MQTT client ID using the AIO key + the date and time the sketch
// was compiled (so this should be unique across multiple devices for a user,
// alternatively you can manually set this to a GUID or other random value).
const char MQTT_CLIENTID[] PROGMEM  = __TIME__ AIO_USERNAME;
const char MQTT_USERNAME[] PROGMEM  = AIO_USERNAME;
const char MQTT_PASSWORD[] PROGMEM  = AIO_KEY;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&aioClient, MQTT_SERVER, AIO_SERVERPORT, MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD);

/****************************** Feeds ***************************************/
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
const char TEMPFG_FEED[] PROGMEM = AIO_USERNAME "/feeds/tempf-g";
Adafruit_MQTT_Publish tempfg = Adafruit_MQTT_Publish(&mqtt, TEMPFG_FEED);

const char HUMG_FEED[] PROGMEM = AIO_USERNAME "/feeds/hum-g";
Adafruit_MQTT_Publish humg = Adafruit_MQTT_Publish(&mqtt, HUMG_FEED);

const char DOORG1_FEED[] PROGMEM = AIO_USERNAME "/feeds/door-g1";
Adafruit_MQTT_Publish doorg1 = Adafruit_MQTT_Publish(&mqtt, DOORG1_FEED);

const char DOORG2_FEED[] PROGMEM = AIO_USERNAME "/feeds/door-g2";
Adafruit_MQTT_Publish doorg2 = Adafruit_MQTT_Publish(&mqtt, DOORG2_FEED);

long now = 0;
long lastMsg = 0;
char msg[150];
int value = 0;

void setup() {
  pinMode(CarPin, INPUT);
  pinMode(TruckPin, INPUT);
  
  // Init sensor
  dht.begin();
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  WiFi.hostname( "ESP8266-Garage" );
  scan_wifi();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  //Setup UDP port for NTP
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  
  // upload the temp and humidity sensor values every 60 seconds 
  uploadTicker.attach(60, setReadyForSensorUpload);

  // udpate the running array of sensor values every 6 seconds
  readingTicker.attach(6, setReadyForSensorReading);
}

void scan_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      delay(10);
    }
  }
  Serial.println("");
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  for (int i = 0; i<numssid; i++) {
    int x = 0;
    // Try to connect to each access point for 10 seconds
    while ((WiFi.status() != WL_CONNECTED) && (x<20)) {
      Serial.println();
      Serial.print("Connecting to ");
      Serial.println(ssid[i]);

      WiFi.begin(ssid[i], password[i]);
      while ((WiFi.status() != WL_CONNECTED) && (x<20)) {
        x++;
        delay(500);
        Serial.print(".");
      }
    }
    if (WiFi.status() != WL_CONNECTED) {
      // If we got here the last connection failed so stop trying
      WiFi.disconnect();
      Serial.println("");
      Serial.print("Failed to connect to ");
      Serial.println(ssid[i]);
    }else{
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      return;
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if ((char)payload[0] == '?') {
    int eT = getEpochTime();
    sendOwnTracksMsg("Status", "Perimeter Check", "fa-eye", eT);
    sendHumanMsg("Status", "Perimeter Check", "fa-eye", eT);
  }
}

void reconnect() { 
  // Loop until we're reconnected
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client","testUser","testID")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world from the garage");
      int eT = getEpochTime();
      sendOwnTracksMsg("Reconnect", "Just Reconnected the Garage", "fa-eye", eT);
      // ... and resubscribe
      client.subscribe("owntracks/#");
      client.subscribe("/msg/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      //delay(5000);
    }
  }
  int8_t retval;
  if (!mqtt.connected()) {
    Serial.print("Attempting Adafruit.io MQTT connection ...");
    retval=mqtt.connect();
    if (retval != 0) {
      Serial.println(mqtt.connectErrorString(retval));
      Serial.println("Retrying Adafruit.io MQTT connection in 5 seconds...");
      mqtt.disconnect();
      //delay(5000);
    }else{
      Serial.println("Connected to Adafruit.io!");
    }
  }
} //reconnect

unsigned long getEpochFromNTPpacket() {
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);


    // print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second

    return epoch;
} //getEpochFromNTPpacket

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
} //sendNTPpacket

int getEpochTime() {
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  
  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    // wait to see if a reply is available
    delay(1000);
  
    int cb = udp.parsePacket();
    if (!cb) {
      Serial.print("no packet yet");
      while (!cb) {
        delay(1000);
        cb = udp.parsePacket();
        Serial.print(".");
        now = millis();
        if (now - lastMsg >10000) {
          sendNTPpacket(timeServerIP);
          lastMsg = now;
        }
      }
      Serial.println("\nPacket Received, length = " + String(cb));
    }
    int epoch = getEpochFromNTPpacket();
    return epoch;
} //getEpochTime

void sendOwnTracksMsg(String title, String desc, String icon, int epoch) {
    //snprintf (msg, 75, "hello world #%ld", value);
    String json = "{"
                     "\"_type\":\"msg\","
                     "\"prio\":2,"  //Priority color 0=blue, 1=yellow, 2=red
                     "\"Title\":\"" + title + "\","
                     "\"desc\":\"" + desc + "\","
                     "\"icon\":\"" + icon + "\","
                     "\"tst\":" + String(epoch) + "} ";
    json.toCharArray(msg,json.length());
    Serial.print("Publish message: ");
    Serial.println(msg);
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    //dtostrf(humidity_data, 4, 2, msg);
    client.publish("owntracks/Keith/Nexus6/msg", msg);
    client.loop();
    client.publish("/msg", msg);
    client.loop();
} //sendOwnTracksMsg

void sendHumanMsg(String title, String desc, String icon, int epoch) {
    String humanMsg = "The UTC time is ";       // UTC is the time at Greenwich Meridian (GMT)
    humanMsg += (epoch  % 86400L) / 3600;
    humanMsg += ":";
    // print the hour (86400 equals secs per day)
    if ( ((epoch % 3600) / 60) < 10 ) {
              // In the first 10 minutes of each hour, we'll want a leading '0'
           humanMsg += "0";
    }
    humanMsg += ((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    humanMsg += ':';
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      humanMsg += '0';
    }
    humanMsg += epoch % 60;
    humanMsg += " Perimeter State: H="; 
    humanMsg += carDoorState;
    humanMsg += " F=";
    humanMsg += truckDoorState;
    humanMsg += " ";
    humanMsg.toCharArray(msg,humanMsg.length());
    Serial.print("Publish message: ");
    Serial.println(msg);
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    unsigned int hMsgLen = humanMsg.length();
    client.publish("/msg", (const uint8_t*)msg, hMsgLen, true);
    client.loop();
} //sendHumanMsg

void loop() {
  while (WiFi.status() != WL_CONNECTED){
    setup_wifi();
  }
  
  if (!client.connected()) {
    reconnect();
  }

  if (!mqtt.connected()) {
    reconnect();
  }
  
  client.loop();

  if (readyForSensorReading) {
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
  }

  if (readyForSensorUpload) {
    readyForSensorUpload = false;
    Serial.println("Uploading Sensor Median.");
    uploadDHTSensor();
    humSamples.clear();
    tempCSamples.clear();
    tempFSamples.clear();
  }

  int doorState = 0;

  //Check the Car door
  doorState = digitalRead(CarPin);
  // If the Car door state changes either way send a message and record new state
  if ((carDoorState != doorState) && (doorState == HIGH)) {
    int eT = getEpochTime();
    carDoorState = doorState;
    sendOwnTracksMsg("Car Door", "Car Garage Door Opened", "fa-eye", eT);
    sendHumanMsg("Car Door", "Car Garage Door Opened", "fa-eye", eT);
    client.publish("doors/car/", "Open");
    doorg1.publish("Open");
  }else if ((carDoorState != doorState) && (doorState == LOW)) {
    int eT = getEpochTime();
    carDoorState = doorState;
    sendOwnTracksMsg("Car Door", "Car Garage Door Closed", "fa-eye", eT);
    sendHumanMsg("Car Door", "Car Garage Door Closed", "fa-eye", eT);
    client.publish("doors/car/", "Closed");
    doorg1.publish("Closed");
  }

  //Check the Truck door
  doorState = digitalRead(TruckPin);
  // If the Truck door state changes either way send a message and record new state
  if ((truckDoorState != doorState) && (doorState == HIGH)) {
    int eT = getEpochTime();
    truckDoorState = doorState;
    sendOwnTracksMsg("Truck Door", "Truck Garage Door Opened", "fa-eye", eT);
    sendHumanMsg("Truck Door", "Truck Garage Door Opened", "fa-eye", eT);
    client.publish("doors/truck/", "Open");
    doorg2.publish("Open");
  }else if ((truckDoorState != doorState) && (doorState == LOW)) {
    int eT = getEpochTime();
    truckDoorState = doorState;
    sendOwnTracksMsg("Truck Door", "Truck Garage Door Closed", "fa-eye", eT);
    sendHumanMsg("Truck Door", "Truck Garage Door Closed", "fa-eye", eT);
    client.publish("doors/truck/", "Closed");
    doorg2.publish("Closed");
  }
}

void setReadyForSensorUpload() {
  readyForSensorUpload = true;
}

void setReadyForSensorReading() {
  readyForSensorReading = true;
}

void uploadDHTSensor() {
  humidity_data = humSamples.getMedian();
  tempF_data = tempFSamples.getMedian();
  tempC_data = tempCSamples.getMedian();

  String json = "{\"humidity\":\"" + String(humidity_data) + "\",\"tempf\":\"" + String(tempF_data) + "\",\"tempc\":\"" + String(tempC_data) + "\"} ";
  json.toCharArray(msg,json.length());
  Serial.print("Publish message: ");
  Serial.println(msg);
    
  dtostrf(humidity_data, 4, 2, msg);
  client.publish("sensors/humidity/gar", msg);
  humg.publish(humidity_data); // publish to Adafruit.io
    
  dtostrf(tempF_data, 4, 2, msg);
  client.publish("sensors/farenheit/gar", msg);
  tempfg.publish(tempF_data); // publish to Adafruit.io
    
  dtostrf(tempC_data, 4, 2, msg);
  client.publish("sensors/celcius/gar", msg);
}
