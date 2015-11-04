/*
Designed by: Keith McGerald
This case was designed as an alternative to the laser cut acrylic case originally 
proposed to enclose these parts according to this tutorial. 
https://learn.adafruit.com/huzzah-weather-display  

Note that I'm not sticking the male right angle headers on my ESP8266. I'm using 
the Micro USB breakout to connect 5V,GND, RX and TX and I have a USB micro to FTDI 
adaptor that I made by using the USB Data+ and Data- lines for the FTDI TX and RX 
lines. I've printed this on my hand built Prusa Air 2 in ABS and it fits well. I had a 
few pegs that had issues so I've brushed all of them with Acetone to reenforce the 
bonding and smooth them out. I've added in a space for installing a DHT11 temperature 
and humidity sensor so that I can measure the "weather" in the room and report it up to 
my home monitoring.

I should have made better use of variables when calculating the size of parts of the 
case but I was in a rush and "would go back and fix it later." It's not later yet. I'd 
also like to build in some support blocks for holding the USB breakout better. I've test 
fit a DHT11 sensor in the print but I haven't wired one up so I don't know if I need to 
add more room for wire routing.
*/

$fn = 50;

// Flags for rendering case or electronic parts
renderbezel = 1;        //Front of the case around the display
renderback = 1;         //Back of the case around the USB and ESP8266
renderelectronics = 1;  //All of the electronic parts
renderUSB = 0;          //MicroUSB breakout
renderHUZZAH = 0;       //ESP8266 HUZZAH module
renderOLED = 0;         //OLED display
renderDHT11 = 0;        //DHT11 temperature and humidity sensor
withDHT11 = 1;          //Make cutout for DHT11

huzwidth = 26;
huzheight = 38.5;
huzthick = 3; //includes smt bits and esp8266 pcb

huzcanwidth = 12.5;
huzcanheight = 15.5;
huzcanthick = 2.5; //the silver can above the pcb

usbwidth = 10.25;
usbheight = 20.5;
usbthick = 1;

usbjackwidth = 6;
usbjackheight = 8;
usbjackthick = 3;

oledpcbwidth = 35.5;
oledpcbheight = 35.5;
oledpcbthick = 2.5;

oledwidth = 34.5;
oledheight = 23;
oledthick = 1.5;

dht11width = 12.75;
dht11height = 16;
dht11thick = 5.85;


////////// Main ///////////


///// Front half of the unit /////

if(renderbezel) {
  color([1,.75,.75]) translate([-5,oledpcbheight+7,0]){
    difference(){
      cube([huzheight+usbwidth+11, oledpcbheight+10, oledpcbthick+oledthick+2]);
      translate([(usbwidth+huzheight+1)/2-oledpcbwidth/2+4,4,-1]){
        difference(){
          cube([oledpcbwidth+2,oledpcbheight+2,oledpcbthick+2]);
          translate([4,4,0])
            cylinder(r=1,h=oledpcbthick+oledthick);
          translate([4,oledpcbheight-2,0])
            cylinder(r=1,h=oledpcbthick+oledthick);
          translate([oledpcbwidth-2,4,0])
            cylinder(r=1,h=oledpcbthick+oledthick);
          translate([oledpcbwidth-2,oledpcbheight-2,0])
            cylinder(r=1,h=oledpcbthick+oledthick);
        }
        translate([0,6.5,0])
          cube([oledpcbwidth+2,oledheight+2,oledpcbthick+oledthick+12]);
      }
    }//difference
  }//color
}//renderbezel

if(renderOLED || renderelectronics) {
  translate([(usbwidth+huzheight+1)/2-oledpcbwidth/2,oledpcbheight+12,0.1])
    drawOLED();
}//renderOLED


///// Back half of the unit /////

if(renderback) {
  color([1,.75,.75])translate([-5,-5,0]){
    difference(){
      cube([huzheight+usbwidth+11, oledpcbheight+10, huzthick+huzcanthick+2]);
      translate([4,4,-1]){
        //cutout for huzzah and usb boards
        cube([huzheight+usbwidth+3,huzwidth+2,huzthick+huzcanthick+2]);
        //cutouts for the USB cable
        translate([0,huzwidth/2-4,.5])
          cube([huzheight+usbwidth+10,usbjackheight+2,usbthick+usbjackthick+3]);
        translate([huzheight+usbwidth+4,huzwidth/2-6,-1])
          cube([6,14,10]);
        if (withDHT11) {
          //cutout for the DHT11
          translate([huzheight-4.5,huzwidth+1,0]){
            cube([dht11height+1,dht11width+1,huzthick+huzcanthick+10]);
            translate([-9,0,0])
              cube([10,dht11width-2, huzthick+huzcanthick+1]);
          }
        }//withDHT11
      }
    }//difference

    //pegs for huzzah module
    translate([7.65,7.65,0])
      cylinder(r=1,h=huzthick+huzcanthick+1);
    translate([7.65,huzwidth+5-2.65,0])
      cylinder(r=1,h=huzthick+huzcanthick+1);
    translate([huzheight+5-2.65,7.65,0])
      cylinder(r=1,h=huzthick+huzcanthick+1);
    translate([huzheight+5-2.65,huzwidth+5-2.65,0])
      cylinder(r=1,h=huzthick+huzcanthick+1);

    //pegs for USB breakout
    translate([huzheight+5+1+usbwidth-2.5,2.75+usbheight/2-(huzwidth-usbheight)/2,-.1])
      cylinder(r=1,h=huzthick+huzcanthick+1);
    translate([huzheight+5+1+usbwidth-2.5,2.5+usbheight+(huzwidth-usbheight)/2,-.1])
      cylinder(r=1,h=huzthick+huzcanthick+1);
  }//color
}//renderback

if(renderHUZZAH || renderelectronics) {
  translate([0,huzwidth,0.1]){
    rotate([0,0,270])
      drawHuzzah();
  }
}//renderHUZZAH

if(renderUSB || renderelectronics) {
  translate([usbwidth+huzheight+1,usbheight+(huzwidth-usbheight)/2,0.1]){
    rotate([0,0,180])
      drawUSB();
  }
}//renderUSB

if(renderDHT11 || renderelectronics) {
  translate([huzheight-5,dht11width+huzwidth+0,huzthick-.5]) {
    rotate([0,0,270])
      drawDHT11();
  }
}//renderDHT11


////////// Modules ///////////

module drawDHT11() {
  
  union(){
    color([0.25,0.5,1])
      cube([dht11width, dht11height, dht11thick]); //sensor
    //sensor leads
    translate([2.25,-7.25,2.5])
      color([.75,.75,.75])
        cube([0.55,7.25,0.25]);
    translate([4.5,-7.25,2.5])
      color([.75,.75,.75])
        cube([0.55,7.25,0.25]);
    translate([7,-7.25,2.5])
      color([.75,.75,.75])
        cube([0.55,7.25,0.25]);
    translate([9.5,-7.25,2.5])
      color([.75,.75,.75])
        cube([0.55,7.25,0.25]);
  }//union
}

module drawUSB() {
  holeoff = 2.5;
  difference() {
    union() {
      color([0,0,1]) cube([usbwidth,usbheight,usbthick]);
      translate([-.5,6.35,usbthick]) 
        color([1,1,1]) cube([usbjackwidth,usbjackheight,usbjackthick]);
    }//union
    translate([holeoff,holeoff,-1])
      cylinder(r=1.25, h=usbthick+2);
    translate([holeoff,usbheight-holeoff,-1])
      cylinder(r=1.25, h=usbthick+2);
  }//difference
}

module drawOLED() {
  holeoff = 3;
  difference() {
    union() {
      // display pcb
      color([0,0,1]) cube([oledpcbwidth,oledpcbheight,oledpcbthick]);
      // add display glass
      translate([.5,6.5,oledpcbthick]) 
        color([0,0,0])cube([oledwidth,oledheight,oledthick]);
    }//union
    // cutout on one side
    translate([12.5/2,oledpcbheight-6.1,-1])
      cube([23,6.2,oledpcbthick+2]);
    // cut mounting holes
    translate([holeoff,holeoff,-1])
      cylinder(r=1.25, h=oledpcbthick+2);
    translate([holeoff,oledpcbheight - holeoff,-1])
      cylinder(r=1.25, h=oledpcbthick+2);
    translate([oledpcbwidth - holeoff,holeoff,-1])
      cylinder(r=1.25, h=oledpcbthick+2);
    translate([oledpcbwidth - holeoff,oledpcbheight - holeoff,-1])
      cylinder(r=1.25, h=oledpcbthick+2);
  }//difference
}

module drawHuzzah() {
  holeoff = 2.65;
  difference() {
   union() {
    // huzzah pcb and surface mount components
    color([0,0,1]) cube([huzwidth, huzheight, huzthick]);
    //silver can over esp8266 chip
    translate([huzwidth/4,7.9,huzthick]) 
      color([1,1,1]) cube([huzcanwidth, huzcanheight, huzcanthick]);
    // GPIO0 button
    translate([8.25,32,0]) 
      color([0,0,0]) cylinder(r=.9,h=3.6);
    // Reset button
    translate([15.75,32,0]) 
      color([0,0,0]) cylinder(r=.9,h=3.6);
   }//union
   // cut mounting holes
   translate([holeoff,holeoff,-1])
    cylinder(r=1.25, h=huzthick+2);
   translate([holeoff,huzheight - holeoff,-1])
    cylinder(r=1.25, h=huzthick+2);
   translate([huzwidth - holeoff,holeoff,-1])
    cylinder(r=1.25, h=huzthick+2);
   translate([huzwidth - holeoff,huzheight - holeoff,-1])
    cylinder(r=1.25, h=huzthick+2);
  }//difference
}
