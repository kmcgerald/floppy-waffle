//Light Dome
//fades all pixels subtly
//code by Tony Sherwood for Adafruit Industries
//and then hacked on by Keith McGerald
 
#include <Adafruit_NeoPixel.h>
 
#define PIN 1

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)

Adafruit_NeoPixel strip;
int pixColors = 4; // Number of color LEDs in the NeoPixel, RGB = 3, RGBW = 4
int numPix = 7; // Number of NeoPixels
int fadeDelay = 500; // Milliseconds between color changes
int alpha = 25; // Current value of the pixels
int color = 3; // The color mode to start in before the button gets pushed
const int buttonPin = 2;
int buttonState = 0; 
int lastButtonState = LOW;
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 50;    // the debounce time; increase if the output flickers

void setup() {
  if(pixColors == 4){
    strip = Adafruit_NeoPixel(numPix, PIN, NEO_GRBW + NEO_KHZ800);
  }else{
    strip = Adafruit_NeoPixel(numPix, PIN, NEO_GRB + NEO_KHZ800);
  }

  strip.begin();
  //pinMode(buttonPin, INPUT);
  if (pixColors == 4) {
    colorWipe(strip.Color(0,0,0,alpha)); // start off white
  }else{
    colorWipe(strip.Color(alpha,alpha,alpha)); // start off white
  }
  strip.show(); // Initialize all pixels to 'off'
  delay(1000);
}
 
void loop() {
 
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:

    // if the button state has changed:    
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        //color++;
      }
    }
  }

  if (color == 0) {
    if (pixColors == 4) {
      colorWipe(strip.Color(0,0,0,alpha));  // white
    }else{
      colorWipe(strip.Color(255,255,255));
    }
  } else if (color == 1) {
    colorWipe(strip.Color(alpha,0,alpha/2)); // pink
  } else if (color == 2) {
    colorWipe(strip.Color(alpha,0,alpha)); // purple
  } else if (color == 3) {
    // red=>pink=>purple=>blue=>purple=>pink=>red
    int red = alpha;
    int blue = 0;
    colorWipe(strip.Color(red,0,blue));
    for(uint16_t i=0; i<alpha; i++) { // red to blue
      delay(fadeDelay);
      red--;
      blue++;
      colorWipe(strip.Color(red,0,blue));
    }
    for(uint16_t i=0; i<alpha; i++) { // blue to red
      delay(fadeDelay);
      red++;
      blue--;
      colorWipe(strip.Color(red,0,blue));
    }
    delay(fadeDelay * 5);
  }else {
    color = 0;
  }

  lastButtonState = reading;
  // Change the line below to alter the color of the lights
  // The numbers represent the Red, Green, and Blue values
  // of the lights, as a value between 0(off) and 1(max brightness)
  //
  // EX:
  // colorWipe(strip.Color(alpha, 0, alpha, 0)); // Pink
  // colorWipe(strip.Color(0, 0, alpha)); // Blue
}
 
// Fill the dots one after the other with a color
void colorWipe(uint32_t c) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
  }
}
