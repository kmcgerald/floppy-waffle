// HOSTNAME for OTA update
#define HOSTNAME "ESP8266-OTA-Widget"
// Port defaults to 8266
// ArduinoOTA.setPort(8266);

// Hostname defaults to esp8266-[ChipID]
// ArduinoOTA.setHostname("myesp8266");

// No authentication by default
// ArduinoOTA.setPassword("admin");

// Password can be set with it's md5 value as well
// MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
// ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

// Update these with values suitable for your network.
const char* ssid[] = {"MyWIFIAP","MyBackupAP"};
const char* password[] = {"The Secure Passphrase I should have","The Secure Passphrase for my backup AP"};
const int numssid = 2; // How many SSIDs in the array above

// Update with you MQTT broker IP
const char* mqtt_server = "333.444.555.666";

// MQTT Client Settings
const char* mqttName = "ESP8266Client_widget";
const char* mqttUser = "Me";
const char* mqttPassword = "shhh tell no one";
const char* mqttAnnounceTopic = "outTopic";
const char* mqttAnnounceMsg = "hello world from the widget";

// OwnTracks used to support messaging over MQTT
const char* ownTracksMsgTopic = "owntracks/Me/myphone/msg";


