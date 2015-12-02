/* This sketch is my attempt at replacing the motherboard of a defunct Brinks/ADT alarm system with an ESP8266 module.
 *  The module will monitor the door sensors and if any of them open (normally closed) it will send a specially formatted
 *  JSON message to my MQTT broker (mosquitto on a Beagle Bone Black) which will be relayed to the OwnTracks client on
 *  my Android phone.  Since the OwnTracks message format requires a UNIX epoch formatted timestamp, I get to roll in some
 *  NTP over UDP for fun.  I found an example of NTP over UDP for ESP8266 here...
 *  https://github.com/sandeepmistry/esp8266-Arduino/blob/master/esp8266com/esp8266/libraries/ESP8266WiFi/examples/NTPClient/NTPClient.ino
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h> // Needed for MQTT messaging
#include <WiFiUdp.h>  // Needed for NTP over UDP

#define MQTT_VERSION MQTT_VERSION_3_1  //Override (lower) the version for older Mosquitto 1.2.2 on BBB

// Update these with values suitable for your network.
/*const char* ssid = "RestrictedAccess";
const char* password = "";
*/

//Primary wifi network
const char* ssid = "YourSSID";
const char* password = "YOUR_PASSWORD";

// Setting up UDP stuff for NTP
unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

const char* mqtt_server = "192.168.1.175";

WiFiClient espClient;
PubSubClient client(espClient);

long now = 0;
long lastMsg = 0;
char msg[150];

// Setup pin assignments for each sensor going down the side of the Huzzah ESP8266
const int garageDoor = 13;
const int frontDoor = 12;
const int basementDoor = 14;
const int slidingDoor = 16;
const int garageCar = 4;
const int garageTruck = 5;

// Sensor state trackers
int garageDoorState = 0;
int frontDoorState = 0;
int basementDoorState = 0;
int slidingDoorState = 0;
int garageCarState = 0;
int garageTruckState = 0;

void setup() {
  pinMode(garageDoor, INPUT);
  pinMode(frontDoor, INPUT);
  pinMode(basementDoor, INPUT);
  pinMode(slidingDoor, INPUT);
  pinMode(garageCar, INPUT);
  pinMode(garageTruck, INPUT);
  
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  scan_wifi();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
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
} //scan_wifi

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
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

  //Setup UDP port for NTP
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() { 
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client","testUser","testpasswd")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("owntracks/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

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

void sendOwnTracksMsg(String title, String desc, String icon) {
  sendNTPpacket(timeServer); // send an NTP packet to a time server
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
          sendNTPpacket(timeServer);
          lastMsg = now;
        }
      }
      Serial.println("\nPacket Received, length = " + String(cb));
    }
    int epoch = getEpochFromNTPpacket();
    
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
    
    // Lets send a human readable version too 
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
    humanMsg += epoch % 60 + " "; // print the second and a trailing space for good measure
    humanMsg.toCharArray(msg,humanMsg.length());
    Serial.print("Publish message: ");
    Serial.println(msg);
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    client.publish("/msg", msg);
    client.loop();
} //sendOwnTracksMsg

void loop() {
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  int doorState = 0;

  //Check the Garage door
  doorState = digitalRead(garageDoor);
  // If the Garage door state changes either way send a message and record new state
  if ((garageDoorState != doorState) && (doorState == HIGH)) {
    sendOwnTracksMsg("Garage Door", "Garage Door Opened", "fa-eye");
    garageDoorState = doorState;
  }else if ((garageDoorState != doorState) && (doorState == LOW)) {
    sendOwnTracksMsg("Garage Door", "Garage Door Closed", "fa-eye");
    garageDoorState = doorState;
  }

  //Check the Front door
  doorState = digitalRead(frontDoor);
  // If the Front door state changes either way send a message and record new state
  if ((frontDoorState != doorState) && (doorState == HIGH)) {
    sendOwnTracksMsg("Front Door", "Front Door Opened", "fa-eye");
    frontDoorState = doorState;
  }else if ((frontDoorState != doorState) && (doorState == LOW)) {
    sendOwnTracksMsg("Front Door", "Front Door Closed", "fa-eye");
    frontDoorState = doorState;
  }
}
