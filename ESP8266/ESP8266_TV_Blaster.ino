/* This sketch is to receive an MQTT message or Alexa voice command and shut off a Samsung TV via IR blaster.
 * I'm using the WeenyMo library to emulate a Belkin Wemo plug in order to achieve Alexa integration since I 
 * already have other Wemo devices. For the hardware I'm using an Adafruit ESP8266 HUZZAH breakout board, a 
 * 2N2222 transitor, and a pair of 20 degree high power IR LEDs.  Bascially I'm using the simple schematic 
 * found here https://create.arduino.cc/projecthub/BuddyC/wifi-ir-blaster-af6bca
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>  // Needed for OTA
#include <ArduinoOTA.h>
#include <PubSubClient.h> // Needed for MQTT messaging
#include <IRremoteESP8266.h> //https://github.com/crankyoldgit/IRremoteESP8266
#include <IRsend.h>
#include "CustSetup.h" //This is where I store secrets like wifi and mqtt passwords
#include "weenyMo.h" //https://github.com/philbowles/weenymo

const uint16_t kIrLed = 16;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRsend irsend(kIrLed);  // Set the GPIO to be used to sending the message.
WiFiClient espClient;
PubSubClient client(espClient);

//
// This function will be called when Alexa hears "switch [on | off] 'vee three'" 
// ESP8266 builtin LED is "backwards" i.e. active LOW, hence Alexa's ON=1 needs reversing
//
void onVoiceCommand(bool onoff){ 
  Serial.printf("onVoiceCommand %d\n",onoff);
  client.publish("/msg", "Toggling the Basement TV");
  Serial.println("Publish message: /msg : Toggling the Basement TV");
  Serial.println("Samsung");
  //samsung tv power on/off https://gist.github.com/Ekus/92842e60e8a3f53f38d3
  uint16_t S_pwr[68]={4600,4350,700,1550,650,1550,650,1600,650,450,650,450,650,450,650,450,700,400,700,1550,650,1550,650,1600,650,450,650,450,650,450,700,450,650,450,650,450,650,1550,700,450,650,450,650,450,650,450,650,450,700,400,650,1600,650,450,650,1550,650,1600,650,1550,650,1550,700,1550,650,1550,650};
  irsend.sendRaw(S_pwr,68,38);
} 
//
bool getAlexaState(){
  Serial.printf("getAlexaState %d\n",!digitalRead(BUILTIN_LED));
  return !digitalRead(BUILTIN_LED);
}  
// The only constructor: const char* name, function<void(bool)> onCommand
// choose the name wisely: Alexa has very selective hearing!
//
weenyMo w("Basement TV",onVoiceCommand);

void setup() {
  // put your setup code here, to run once:
  pinMode(BUILTIN_LED, OUTPUT); // Initialize the BUILTIN_LED pin (0) as an output
  digitalWrite(BUILTIN_LED, LOW); // We're not connected so turn on warning light
  irsend.begin();
  #if ESP8266
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  #else  // ESP8266
  Serial.begin(115200, SERIAL_8N1);
  #endif  // ESP8266
  WiFi.hostname( "ESP8266-BasementTVBlaster" );
  scan_wifi();
  setup_wifi();
  w.gotIPAddress(); // ready to roll...Tell Alexa to discover devices.
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // OTA Setup
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  //WiFi.hostname(hostname);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
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
      client.publish("/msg", "Basement TV Blaster is Alive");
      Serial.println("Publish message: /msg : Basement TV Blaster is Alive");
  }
  if ((strcmp(topic,"/device/basement/tv")==0)&&((char)payload[0] == '0')) {
      client.publish("/msg", "Toggling the Basement TV");
      Serial.println("Publish message: /msg : Toggling the Basement TV");
      Serial.println("Samsung");
      //samsung tv power on/off https://gist.github.com/Ekus/92842e60e8a3f53f38d3
      uint16_t S_pwr[68]={4600,4350,700,1550,650,1550,650,1600,650,450,650,450,650,450,650,450,700,400,700,1550,650,1550,650,1600,650,450,650,450,650,450,700,450,650,450,650,450,650,1550,700,450,650,450,650,450,650,450,650,450,700,400,650,1600,650,450,650,1550,650,1600,650,1550,650,1550,700,1550,650,1550,650};
      irsend.sendRaw(S_pwr,68,38);
  }
} //callback

void reconnect() { 
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqttName, mqttUser, mqttPassword)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqttAnnounceTopic, mqttAnnounceMsg);
      client.subscribe("/msg/#");
      client.subscribe("/device/basement/tv/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      //delay(5000);
    }
  }
  digitalWrite(BUILTIN_LED, HIGH); // We're connected turn off warning light
} //reconnect

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
  
  ArduinoOTA.handle();
  client.loop();
}
