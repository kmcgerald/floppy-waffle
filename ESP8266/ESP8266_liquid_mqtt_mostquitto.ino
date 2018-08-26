/* This sketch is for a device I built using an Adafruit ESP8266 HUZZAH board and an 
 *  Optomax Digital Liquid Level Sensor (https://www.adafruit.com/product/3397) to 
 *  alert me to when the drain in my basement walkup stairs might be clogged and the
 *  water level has risen to a point that it's could leak under the door soon. I've 
 *  also added a audio buzzer to sound an alarm just in case the network connection
 *  dropped off. If I upgraded to a Feather HUZZAH I could add a backup LiPo so it 
 *  would work if the power goes out.
 *  
 *  I'm using MQTT for messaging, ArduinoOTA for updating, NTP to grab the time so I 
 *  know when alerts occurred, and I use a Custom Setup file to store my sensitive 
 *  values that I don't want to publish to github.  I've grabbed lots of bits of code
 *  from all over and reworked some of it over time such that I've lost track of who's
 *  it might have started from.
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h> // Needed for MQTT messaging
#include <WiFiUdp.h>  // Needed for NTP over UDP
#include "CustSetup.h"

//#define MQTT_VERSION MQTT_VERSION_3_1  //Override (lower) the version for older Mosquitto 1.2.2 on BBB

// Setting up UDP stuff for NTP
unsigned int localPort = 2390;      // local port to listen for UDP packets
/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
//IPAddress timeServerIP(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

WiFiClient espClient;
PubSubClient client(espClient);

long now = 0;
long lastMsg = 0;
char msg[150];

// Setup pin assignments for each sensor going down the side of the Huzzah ESP8266
const int blueLED = 2; 
const int buzzerPin = 12; 
const int liquidSensor = 14;
// Sensor state trackers
int liquidSensorState = 0; 

void setup() {
  // put your setup code here, to run once:
  pinMode(liquidSensor, INPUT_PULLUP);
  pinMode(blueLED, OUTPUT);  // Initialize the blue LED pin (2) as an output
  pinMode(BUILTIN_LED, OUTPUT); // Initialize the BUILTIN_LED pin (0) as an output
  digitalWrite(BUILTIN_LED, LOW); // We're not connected so turn on warning light
  
  Serial.begin(115200);
  WiFi.hostname( "ESP8266-Walkup" );
  scan_wifi();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  //Setup UDP port for NTP
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  // OTA Setup
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  //WiFi.hostname(hostname);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void scan_wifi() {
  // I like to scan for wifi networks for troubleshooting
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
} //scan_wifi

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
      digitalWrite(BUILTIN_LED, HIGH); // We're connected turn off warning light
      return;
    }
  }
} //setup_wifi

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
    int sensorState = 0;

    //Check the Liquid sensor
    sensorState = digitalRead(liquidSensor);
    if (sensorState == HIGH) {
      client.publish("sensors/liquid/walkup", "Dry");
      Serial.println("Publish message: sensors/liquid/walkup : Dry");
    }else if (sensorState == LOW) {
      client.publish("sensors/liquid/walkup", "Wet");
      Serial.println("Publish message: sensors/liquid/walkup : Wet");
    }
  }
} //callback

void reconnect() { 
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqttName, mqttUser, mqttPassword)) {
    //if (client.connect("ESP8266Client","testUser","testID")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqttAnnounceTopic, mqttAnnounceMsg);
      int eT = getEpochTime();
      sendOwnTracksMsg("Reconnect", "Just Reconnected", "fa-eye", eT);
      // ... and resubscribe
      client.subscribe("owntracks/#");
      client.subscribe("/msg/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  digitalWrite(BUILTIN_LED, HIGH); // We're connected turn off warning light
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
  WiFi.hostByName(ntpServerName, timeServerIP);
  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    // wait to see if a reply is available
    delay(2000);
  
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
    client.publish(ownTracksMsgTopic, msg);
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
    humanMsg += " Perimeter State: L="; 
    humanMsg += liquidSensorState;
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
  // put your main code here, to run repeatedly:
  while (WiFi.status() != WL_CONNECTED){
    digitalWrite(BUILTIN_LED, LOW); //We're not connected so turn on warning light (active low)
    setup_wifi();
  }
  
  if (!client.connected()) {
    digitalWrite(BUILTIN_LED, LOW); //We're not connected so turn on warning light (active low)
    reconnect();
  }

  client.loop();

  ArduinoOTA.handle();

  int sensorState = 0;

  //Check the Liquid sensor
  sensorState = digitalRead(liquidSensor);
  // If the Liquid Sensor state changes either way send a message and record new state
  if ((liquidSensorState != sensorState) && (sensorState == HIGH)) { // Sensor is High and dry
    digitalWrite(blueLED, HIGH);   // Turn the LED off (Note that HIGH is the voltage level
    // but actually the LED is off; this is because it is acive low on the ESP-01)
    noTone(buzzerPin);
    Serial.println("stopping tone");
    int eT = getEpochTime();
    liquidSensorState = sensorState;
    sendOwnTracksMsg("Walkup Liquid Sensor", "Walkup Liquid Sensor Dry", "fa-eye", eT);
    sendHumanMsg("Walkup Liquid Sensor", "Walkup Liquid Sensor Dry", "fa-eye", eT);
    client.publish("sensors/liquid/walkup", "Dry");
  }else if ((liquidSensorState != sensorState) && (sensorState == LOW)) { // Low is Liquid!!
    digitalWrite(blueLED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because it is acive low on the ESP-01)
    tone(buzzerPin, 440);
    Serial.println("calling tone");
    int eT = getEpochTime();
    liquidSensorState = sensorState;
    sendOwnTracksMsg("Walkup Liquid Sensor", "Walkup Liquid Sensor Wet", "fa-eye", eT);
    sendHumanMsg("Walkup Liquid Sensor", "Walkup Liquid Sensor Wet", "fa-eye", eT);
    client.publish("sensors/liquid/walkup", "Wet");
  }
}
