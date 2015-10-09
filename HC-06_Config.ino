//This is a simple sketch to setup an HC-06 Bluetooth module via an Arduino and the IDE Serial Monitor
//Connect to the arduino at 9600 with No line ending
//Connect the power and ground of the HC-06 to the Arduino VCC and GND and connect the module's RX to pin 11 and TX to pin 10
//Try different baud rates for "mySerial" until sending an "AT" command produces an "OK" response.
/*
AT : check the connection
AT+NAME: Change name. No space between name and command.
AT+BAUD: change baud rate, x is baud rate code, no space between command and code.
AT+PIN: change pin, xxxx is the pin, again, no space.
AT+VERSION

AT+NAMEPROTOTYPE will set the name to PROTOTYPE. 

To change baud rate, type AT+BAUDX, where X=1 to 9.
1 set to 1200bps
2 set to 2400bps 
3 set to 4800bps 
4 set to 9600bps (Default) 
5 set to 19200bps 
6 set to 38400bps 
7 set to 57600bps 
8 set to 115200bps
so sending AT+BAUD4 will set the baud rate to 9600.
*/
#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11); // RX, TX

void setup() {

  Serial.begin(9600);

  pinMode(9,OUTPUT); digitalWrite(9,HIGH);

  Serial.println("Enter AT commands:");

  mySerial.begin(57600);

}

void loop() {

  if (mySerial.available())
    Serial.write(mySerial.read());

  if (Serial.available())
    mySerial.write(Serial.read());

}
