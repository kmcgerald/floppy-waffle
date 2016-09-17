/* 
*  This is a basic script to get the ESP8266 module up and running on a wifi network.  It starts up, scans for networks, prints 
*  scan results to serial, and connects to one you gave it ahead of time. The ESP8266WiFi library was downloaded from Adafruit.com
*  Most of this code isn't my own but I've lost track of where I got it from. Probaly Adafruit or Sparkfun. 
*  If I find it again I'll credit the sites here
*/

#include <ESP8266WiFi.h>

// Update these with values suitable for your network.
const char* ssid[] = {"MyWIFIAP","MyBackupAP"};
const char* password[] = {"The Secure Passphrase I should have","The Secure Passphrase for my backup AP"};
const int numssid = 2; // How many SSIDs in the array above

WiFiClient espClient;

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  WiFi.hostname( "ESP8266-Test" );
  scan_wifi();
  while (WiFi.status() != WL_CONNECTED){
    setup_wifi();
  }
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

void loop() {
  // Do Something
}
