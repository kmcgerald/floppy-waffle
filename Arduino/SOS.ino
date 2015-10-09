/*
  SOS
  I based this on the standar Arduio Blink ("hello world") sketch but instead I decided to have more fun and blink SOS in 
  Morse code, making heavy use of methods/functions so a beginner will learn a little more.  It might have been more appropriate
  to blink "Hello World" in Morse code rather than SOS.  I'll leave that exercise up to the reader.  :)
 */

void setup() {                
  // initialize the digital pin as an output.
  // Pin 13 has an LED connected on most Arduino boards:
  pinMode(13, OUTPUT);     
}

void loop() {
  morse_s();
  delay(500);
  morse_o();
  delay(500);
  morse_s();
  delay(2000);
}

void morse_s () {
  morse_dot();
  morse_dot();
  morse_dot();
}

void morse_o () {
  morse_dash();
  morse_dash();
  morse_dash();
}

void morse_dot() {
  digitalWrite(13,HIGH);
  delay(500);
  digitalWrite(13,LOW);
  delay(500);
}

void morse_dash() {
  digitalWrite(13,HIGH);
  delay(1000);
  digitalWrite(13,LOW);
  delay(500);
}
