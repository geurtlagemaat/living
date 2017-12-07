
#include "LPD8806.h"
#include "SPI.h"

// control LPD8806-based RGB LED Modules in a strip
// Number of RGB LEDs in strand:
int nLEDs = 64;

// Chose 2 pins for output; can be any valid output pins:
int dataPin  = 4; // groen, di op led
int clockPin = 5; // geel, ci op led
String readString;

// First parameter is the number of LEDs in the strand.  The LED strips
// are 32 LEDs per meter but you can extend or cut the strip.  Next two
// parameters are SPI data and clock pins:
LPD8806 strip = LPD8806(nLEDs, dataPin, clockPin);

uint32_t cPurple = strip.Color(127, 5, 127);

// #define DEBUG
byte message[] = {0x00, 0x39, 0x39, 0x39, 0x30, 0x30, 0x32, 0x06, 0x30, 0x30, 0x2b, 0x33, 0x30, 0x30, 0x31, 0x04, 0x30, 0x30, 0x30, 0x04};

char MasterNode[4] = "131";
char myNodeID[4] = "752";

void setup() {
  message[1] = MasterNode[0];
  message[2] = MasterNode[1];
  message[3] = MasterNode[2];
  
  message[4] = myNodeID[0];
  message[5] = myNodeID[1];
  message[6] = myNodeID[2];
  
  Serial.begin(9600);
  #ifdef DEBUG
    Serial.print("NodeID: ");
    Serial.write(myNodeID, sizeof(myNodeID));
    Serial.println("."); 
  #endif
}

void loop() {
  char str[19];   // de hele inkomende message
  byte byte_receive;
  int iCalcChecksum;
  byte state = 0;
  char toNodeID[4];
  char readChecksum[4];

  // check for incoming messages
  int i = 0;
  if (Serial.available()) {
    state = 0;
    iCalcChecksum = 0;
    delay(50); // allows all serial sent to be received together
    while (Serial.available()) {
      byte_receive = Serial.read();
      if (byte_receive == 00) {
        state = 1;
      }
      if ( (state == 1) && (i < 20) ) {
        str[i++] = byte_receive; 
        if (i < 17) {
          iCalcChecksum = iCalcChecksum + byte_receive;
        }
      }
    }
    str[i++] = '\0';
  }
  // Do Pull Messages 
  if (i > 19) {  // some hope for a incoming message...
    readChecksum[0] = str[16];
    readChecksum[1] = str[17];
    readChecksum[2] = str[18];
    readChecksum[3] = '\0';
    String sReadChar (readChecksum);
    
    toNodeID[0] = str[1];
    toNodeID[1] = str[2];
    toNodeID[2] = str[3];
    toNodeID[3] = '\0';

#ifdef DEBUG
    Serial.print("read Checksum: ");
    Serial.println(readChecksum);

    Serial.print("calculated checksum: ");
    Serial.println(iCalcChecksum);
    
    Serial.print("toNodeID: ");
    Serial.println(toNodeID);
#endif
    String sToNodeID(toNodeID);
    String sMyNodeID(myNodeID);
    if (sReadChar.toInt() == iCalcChecksum) {
      // valid checksum
      if ( (sToNodeID == sMyNodeID) ) {
        // voor onze node
        char nodeFunc[3];
        nodeFunc[0]=str[8];
        nodeFunc[1]=str[9];
        nodeFunc[2]='\0';
        String sNodeFunc(nodeFunc);
    
        char nodeValue[4];
        nodeValue[0]=str[12];
        nodeValue[1]=str[13];
        nodeValue[2]=str[14];
        nodeValue[3]='\0';
        String sNodeValue(nodeValue);
        if (str[7] == 5) {  
#ifdef DEBUG
          Serial.println("message ENQ!");
          Serial.print("functie: ");
          Serial.println(sNodeFunc);
#endif          
          if (sNodeFunc=="60"){  
            allOff();
            message[7] = 0x06;  
            message[8] = str[8];
            message[9] = str[9];
          }
          else if (sNodeFunc=="61"){  
            SetTempValue(sNodeValue.toInt());
            message[7] = 0x06; 
            message[8] = str[8];
            message[9] = str[9];
            SendMessage();
          }
          else if (sNodeFunc=="62"){  
            rainbowCycle(0);
            message[7] = 0x06; 
            message[8] = str[8];
            message[9] = str[9];
            SendMessage();
          }
        }
        else if (str[7] == 6) {
#ifdef DEBUG
          Serial.println("message ACK!");
#endif
        } // msg ACK
        else if (str[7] == 0x15) {
#ifdef DEBUG
          Serial.println("message NACK!");
#endif
          // resend
          SendMessage();
        }; // if NACK
      }; // if this node
    }; // if checksum
  }; // if i > 19
}

void SendMessage() {
  InsertMessageChecksum();
  UCSR0A=UCSR0A |(1 << TXC0);
  delay(1);
  Serial.write(message, sizeof(message));
  while (!(UCSR0A & (1 << TXC0)));
  delay(10);
}

void InsertMessageChecksum() {
  // bereken checksum door message array pos 1 t/m. 15 op te tellen. Plaats het checksum getal in de message
  int iChecksum = 0;
  for (int i = 1; i < 16; i++) {
    iChecksum = iChecksum + message[i];
  } // for i

  char newChecksum[4];
  sprintf (newChecksum, "%03i", iChecksum);

  message[16] = newChecksum[0];
  message[17] = newChecksum[1];
  message[18] = newChecksum[2];
} // CalcMessageChecksum

void BoolValueToMessage(boolean Value) {
  message[10] = 0x2B;
  message[11] = 0x33;
  if (Value){
    message[12] = 0x30;
    message[13] = 0x30;
    message[14] = 0x31;
  }
  else{
    message[12] = 0x30;
    message[13] = 0x30;
    message[14] = 0x30;   
  }
}

void allOff(){
  int i;
  for(i=0; i<strip.numPixels(); i++) strip.setPixelColor(i, 0);
};

void SetTempValue(int iNodeValue){
  /* we hebben 64 leds dus goed voor een bereik van -20 t/m. +44 graden celcius
     laatste pixel hangt op de grond, dus numPixels() = -20 */
  int i;
  int x = 0;
  int iNumberLeds = 0;

  for(i=0; i<strip.numPixels(); i++) strip.setPixelColor(i, 0);
    strip.show();
    
  if (iNodeValue<-20){
    iNodeValue =-20;
  };
  if (iNodeValue>43){
    iNodeValue =43;
  };  
  if (iNodeValue < 0){
    iNumberLeds = 21 - abs(iNodeValue);
  }
  else{
    iNumberLeds = 21 + iNodeValue; // de eerste -20 tot 0
  }
#ifdef DEBUG    
  Serial.print(iNodeValue);
  Serial.print(", number of leds: ");
  Serial.println(iNumberLeds);
#endif    
  for (i=strip.numPixels()-1; x<iNumberLeds;i--){
    strip.setPixelColor(i, GetTempColor(i));
    strip.show();
    x=x+1;
    delay(50);
  };
}

uint32_t GetTempColor(uint8_t iLedNr){
  byte r, g, b;
  Serial.print("iLedNr: ");Serial.println(iLedNr);
  if (iLedNr<18){
    float fFact = (127/18);
    byte bLedCorr = (18 -iLedNr);
    float fCalcOffset = fFact * bLedCorr;
    r = 127;
    g = (127-round(fCalcOffset));
    b = 0;
    Serial.print("iLednr: ");Serial.print(bLedCorr);Serial.print("factor: ");Serial.print(fFact);Serial.print(" offset: ");Serial.print(fCalcOffset);Serial.print(" R: ");Serial.print(r);Serial.print(" G: ");Serial.print(g);Serial.print(" B: ");Serial.println(b);
  };
  if (iLedNr >=18 && iLedNr<28){
    float fFact = (127/10);
    byte bLedCorr = (28 -iLedNr);
    float fCalcOffset = fFact * bLedCorr;
    r = round(fCalcOffset);
    g = 127;
    b = 0;
    Serial.print("iLednr: ");Serial.print(bLedCorr);Serial.print("factor: ");Serial.print(fFact);Serial.print(" offset: ");Serial.print(fCalcOffset);Serial.print(" R: ");Serial.print(r);Serial.print(" G: ");Serial.print(g);Serial.print(" B: ");Serial.println(b);
  };
  if (iLedNr >=28 && iLedNr<43){
    float fFact = (127/15);
    byte bLedCorr = (43 -iLedNr);
    float fCalcOffset = fFact * bLedCorr;
    r = 0;
    g = 127;
    b = (127 - round(fCalcOffset));
    Serial.print("iLednr: ");Serial.print(bLedCorr);Serial.print("factor: ");Serial.print(fFact);Serial.print(" offset: ");Serial.print(fCalcOffset);Serial.print(" R: ");Serial.print(r);Serial.print(" G: ");Serial.print(g);Serial.print(" B: ");Serial.println(b);
  };
  if (iLedNr >=43){ 
    float fFact = (127/21);
    byte bLedCorr = (64 -iLedNr);
    float fCalcOffset = fFact * bLedCorr;
    r = 0;
    g = round(fCalcOffset); 
    b = 127;
    Serial.print("iLednr: ");Serial.print(bLedCorr);Serial.print("factor: ");Serial.print(fFact);Serial.print(" offset: ");Serial.print(fCalcOffset);Serial.print(" R: ");Serial.print(r);Serial.print(" G: ");Serial.print(g);Serial.print(" B: ");Serial.println(b);
  };
  return(strip.Color(r,g,b));
};

void rainbow(uint8_t wait) {
  int i, j;
   
  for (j=0; j < 384; j++) {     // 3 cycles of all 384 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel( (i + j) % 384));
    }  
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

// Slightly different, this one makes the rainbow wheel equally distributed 
// along the chain
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;
  
  for (j=0; j < 384 * 5; j++) {     // 5 cycles of all 384 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      // tricky math! we use each pixel as a fraction of the full 384-color wheel
      // (thats the i / strip.numPixels() part)
      // Then add in j which makes the colors go around per pixel
      // the % 384 is to make the wheel cycle around
      strip.setPixelColor(i, Wheel( ((i * 384 / strip.numPixels()) + j) % 384) );
    }  
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

// Fill the dots progressively along the strip.
void colorWipe(uint32_t c, uint8_t wait) {
  int i;

  for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

// Chase one dot down the full strip.
void colorChase(uint32_t c, uint8_t wait) {
  int i;

  // Start by turning all pixels off:
  for(i=0; i<strip.numPixels(); i++) strip.setPixelColor(i, 0);

  // Then display one pixel at a time:
  for(i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c); // Set new pixel 'on'
    strip.show();              // Refresh LED states
    strip.setPixelColor(i, 0); // Erase pixel, but don't refresh!
    delay(wait);
  }

  strip.show(); // Refresh to turn off last pixel
}

/* Helper functions */

//Input a value 0 to 384 to get a color value.
//The colours are a transition r - g -b - back to r

uint32_t Wheel(uint16_t WheelPos)
{
  byte r, g, b;
  switch(WheelPos / 128)
  {
    case 0:
      r = 127 - WheelPos % 128;   //Red down
      g = WheelPos % 128;      // Green up
      b = 0;                  //blue off
      break; 
    case 1:
      g = 127 - WheelPos % 128;  //green down
      b = WheelPos % 128;      //blue up
      r = 0;                  //red off
      break; 
    case 2:
      b = 127 - WheelPos % 128;  //blue down 
      r = WheelPos % 128;      //red up
      g = 0;                  //green off
      break; 
  }
  return(strip.Color(r,g,b));
}


