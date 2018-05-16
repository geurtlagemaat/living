#include <SimpleTimer.h>

/* Connect the DSM501 sensor as follow 
 * https://www.elektronik.ropla.eu/pdf/stock/smy/dsm501.pdf
 * 1 green vert - Not used
 * 2 yellow jaune - Vout2 - 1 microns (PM1.0)
 * 3 white blanc - Vcc
 * 4 red rouge - Vout1 - 2.5 microns (PM2.5)
 * 5 black noir - GND
*/
#define DUST_SENSOR_DIGITAL_PIN_PM10  8        // DSM501 Pin 2 of DSM501 (jaune / Yellow)
#define DUST_SENSOR_DIGITAL_PIN_PM25  5        // DSM501 Pin 4 (rouge / red) 
#define COUNTRY                       1         // 0. France, 1. Europe, 2. USA/China
#define EXCELLENT                     "Excellent"
#define GOOD                          "Bon"
#define ACCEPTABLE                    "Moyen"
#define MODERATE                      "Mediocre"
#define HEAVY                         "Mauvais"
#define SEVERE                        "Tres mauvais"
#define HAZARDOUS                     "Dangereux"

#define pinCONTROL 12   // de pin waarmee we de RS 485 mee besturen (pin 18 fysiek)
// #define DEBUG
byte message[] = {0x00, 0x39, 0x39, 0x39, 0x30, 0x30, 0x32, 0x06, 0x30, 0x30, 0x2b, 0x33, 0x30, 0x30, 0x31, 0x04, 0x30, 0x30, 0x30, 0x04};

char MasterNode[4] = "311";
char myNodeID[4] = "654";

unsigned long   duration;
unsigned long   starttime;
unsigned long   endtime;
unsigned long   lowpulseoccupancy = 0;
float           ratio = 0;
// unsigned long   SLEEP_TIME    = 2 * 1000;       // Sleep time between reads (in milliseconds)
unsigned long   sampletime_ms = 120000;  // Durée de mesure - sample time (ms)

struct structAQI{
  // variable enregistreur - recorder variables
  unsigned long   durationPM10;
  unsigned long   lowpulseoccupancyPM10 = 0;
  unsigned long   durationPM25;
  unsigned long   lowpulseoccupancyPM25 = 0;
  unsigned long   starttime;
  unsigned long   endtime;
  // Sensor AQI data
  float         concentrationPM25 = 0;
  float         concentrationPM10  = 0;
  int           AqiPM10            = -1;
  int           AqiPM25            = -1;
  // Indicateurs AQI - AQI display
  int           AQI                = 0;
  String        AqiString          = "";
  int           AqiColor           = 0;
};
struct structAQI AQI;

SimpleTimer timer;


void ValueToMessage(float Value) {
  // TODO: negatieve waarden
  int iLeft = (int)(Value);
  int iRight = 10000 * (Value - iLeft);

  String sLeft = String(iLeft);
  String sRight = String(iRight);

  if (iLeft > 99) {
    // value without decimal part
    char tmpBuffer[4];
    sLeft.toCharArray(tmpBuffer, sizeof(tmpBuffer));
    message[11] = 0x33;
    message[12] = tmpBuffer[0];
    message[13] = tmpBuffer[1];
    message[14] = tmpBuffer[2];
  }
  else if (iLeft > 9) {
    // two decimals, 1 fraction
    char tmpBuffer[3];
    sLeft.toCharArray(tmpBuffer, sizeof(tmpBuffer));
    message[11] = 0x32;
    message[12] = tmpBuffer[0];
    message[13] = tmpBuffer[1];
    // fraction
    char tmpBufferDec[2];
    sRight.toCharArray(tmpBufferDec, sizeof(tmpBufferDec));
    message[14] = tmpBufferDec[0];
  }
  else if (iLeft > 0) {
    // 1 decimals, 2 fraction
    char tmpBuffer[2];
    sLeft.toCharArray(tmpBuffer, sizeof(tmpBuffer));
    message[11] = 0x31;
    message[12] = tmpBuffer[0];
    // fraction
    char tmpBufferDec[3];
    sRight.toCharArray(tmpBufferDec, sizeof(tmpBufferDec));
    message[13] = tmpBufferDec[0];
    message[14] = tmpBufferDec[1];
  }
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

void SendMessage() {
  InsertMessageChecksum();
  UCSR0A=UCSR0A |(1 << TXC0);
  digitalWrite(pinCONTROL,HIGH);
  delay(1);
  Serial.write(message, sizeof(message));
  while (!(UCSR0A & (1 << TXC0)));
  digitalWrite(pinCONTROL,LOW);
  delay(10);
}
void updateAQILevel(){
  AQI.AQI = AQI.AqiPM10;
}

void updateAQI() {
#ifdef DEBUG 
  Serial.println(" updateAQI");
#endif
  // Actualise les mesures - update measurements
  AQI.endtime = millis();
  float ratio = AQI.lowpulseoccupancyPM10 / (sampletime_ms * 10.0);
  float concentration = 1.1 * pow( ratio, 3) - 3.8 *pow(ratio, 2) + 520 * ratio + 0.62; // using spec sheet curve
  if ( sampletime_ms < 3600000 ) { concentration = concentration * ( sampletime_ms / 3600000.0 ); }
  AQI.lowpulseoccupancyPM10 = 0;
  AQI.concentrationPM10 = concentration;
  
  ratio = AQI.lowpulseoccupancyPM25 / (sampletime_ms * 10.0);
  concentration = 1.1 * pow( ratio, 3) - 3.8 *pow(ratio, 2) + 520 * ratio + 0.62;
  if ( sampletime_ms < 3600000 ) { concentration = concentration * ( sampletime_ms / 3600000.0 ); }
  AQI.lowpulseoccupancyPM25 = 0;
  AQI.concentrationPM25 = concentration;
#ifdef DEBUG
  Serial.print("Concentrations => PM2.5: "); Serial.print(AQI.concentrationPM25); Serial.print(" | PM10: "); Serial.println(AQI.concentrationPM10);
#endif  
  message[8] = 0x32;
  message[9] = 0x33;  // AQI.concentrationPM25 = func 23
  ValueToMessage(AQI.concentrationPM25);
  SendMessage();
  delay(30);
  message[8] = 0x32;
  message[9] = 0x34;  // AQI.concentrationPM10 = func 24
  ValueToMessage(AQI.concentrationPM10);
  SendMessage();
  delay(30);
  
  AQI.starttime = millis();
      
  // Actualise l'AQI de chaque capteur - update AQI for each sensor 
  if ( COUNTRY == 0 ) {
    // France
    AQI.AqiPM25 = getATMO( 0, AQI.concentrationPM25 );
    AQI.AqiPM10 = getATMO( 1, AQI.concentrationPM10 );
  } else if ( COUNTRY == 1 ) {
    // Europe
    AQI.AqiPM25 = getACQI( 0, AQI.concentrationPM25 );
    AQI.AqiPM10 = getACQI( 1, AQI.concentrationPM10 );
  } else {
    // USA / China
    AQI.AqiPM25 = getAQI( 0, AQI.concentrationPM25 );
    AQI.AqiPM10 = getAQI( 0, AQI.concentrationPM10 );
  }

  // Actualise l'indice AQI - update AQI index
  updateAQILevel();
  updateAQIDisplay();
#ifdef DEBUG  
  Serial.print("AQIs => PM25: "); Serial.print(AQI.AqiPM25); Serial.print(" | PM10: "); Serial.println(AQI.AqiPM10);
  Serial.print(" | AQI: "); Serial.println(AQI.AQI); Serial.print(" | Message: "); Serial.println(AQI.AqiString);
#endif
  message[8] = 0x32;
  message[9] = 0x35;  // AQI.AqiPM25 = func 25
  ValueToMessage(AQI.AqiPM25);
  SendMessage();
  delay(30);
  message[8] = 0x32;
  message[9] = 0x36;  // AQI.AqiPM25 = func 26
  ValueToMessage(AQI.AqiPM10);
  SendMessage();
  delay(30);
 
  message[8] = 0x32;
  message[9] = 0x37;  // AQI.concentrationPM10 = func 27
  ValueToMessage(AQI.AQI);
  SendMessage();
}

void setup() {
  message[1] = MasterNode[0];
  message[2] = MasterNode[1];
  message[3] = MasterNode[2];
  
  message[4] = myNodeID[0];
  message[5] = myNodeID[1];
  message[6] = myNodeID[2];

  message[7] = 0x06;  // QOS: all messages are fire once and forget
  
  // RS485 control
  pinMode(pinCONTROL,OUTPUT);
  digitalWrite(pinCONTROL,LOW);
  
  Serial.begin(9600);
  pinMode(DUST_SENSOR_DIGITAL_PIN_PM10,INPUT);
  pinMode(DUST_SENSOR_DIGITAL_PIN_PM25,INPUT);

  // wait 60s for DSM501 to warm up
  for (int i = 1; i <= 60; i++)
  {
    delay(1000); // 1s
#ifdef DEBUG
    Serial.print(i);
    Serial.println(" s (wait 60s for DSM501 to warm up)");
#endif
  }
#ifdef DEBUG
  Serial.println("Init done!"); 
#endif
  AQI.starttime = millis();
  timer.setInterval(sampletime_ms, updateAQI); 
}

void loop() {
  AQI.lowpulseoccupancyPM10 += pulseIn(DUST_SENSOR_DIGITAL_PIN_PM10, LOW);
  AQI.lowpulseoccupancyPM25 += pulseIn(DUST_SENSOR_DIGITAL_PIN_PM25, LOW);
  
  timer.run(); 
}

/*
 * Calcul l'indice de qualité de l'air français ATMO
 * Calculate French ATMO AQI indicator
 */
int getATMO( int sensor, float density ){
  if ( sensor == 0 ) { //PM2,5
    if ( density <= 11 ) {
      return 1; 
    } else if ( density > 11 && density <= 24 ) {
      return 2;
    } else if ( density > 24 && density <= 36 ) {
      return 3;
    } else if ( density > 36 && density <= 41 ) {
      return 4;
    } else if ( density > 41 && density <= 47 ) {
      return 5;
    } else if ( density > 47 && density <= 53 ) {
      return 6;
    } else if ( density > 53 && density <= 58 ) {
      return 7;
    } else if ( density > 58 && density <= 64 ) {
      return 8;
    } else if ( density > 64 && density <= 69 ) {
      return 9;
    } else {
      return 10;
    }
  } else {
    if ( density <= 6 ) {
      return 1; 
    } else if ( density > 6 && density <= 13 ) {
      return 2;
    } else if ( density > 13 && density <= 20 ) {
      return 3;
    } else if ( density > 20 && density <= 27 ) {
      return 4;
    } else if ( density > 27 && density <= 34 ) {
      return 5;
    } else if ( density > 34 && density <= 41 ) {
      return 6;
    } else if ( density > 41 && density <= 49 ) {
      return 7;
    } else if ( density > 49 && density <= 64 ) {
      return 8;
    } else if ( density > 64 && density <= 79 ) {
      return 9;
    } else {
      return 10;
    }  
  }
}

void updateAQIDisplay(){
  /*
   * 1 EXCELLENT                    
   * 2 GOOD                         
   * 3 ACCEPTABLE               
   * 4 MODERATE            
   * 5 HEAVY               
   * 6 SEVERE
   * 7 HAZARDOUS
   */
  if ( COUNTRY == 0 ) {
    // Système ATMO français - French ATMO AQI system 
    switch ( AQI.AQI) {
      case 10: 
        AQI.AqiString = SEVERE;
        break;
      case 9:
        AQI.AqiString = HEAVY;
        break;
      case 8:
        AQI.AqiString = HEAVY;
        break;  
      case 7:
        AQI.AqiString = MODERATE;
        break;
      case 6:
        AQI.AqiString = MODERATE;
        break;   
      case 5:
        AQI.AqiString = ACCEPTABLE;
        break;
      case 4:
        AQI.AqiString = GOOD;
        break;
      case 3:
        AQI.AqiString = GOOD;
        break;
      case 2:
        AQI.AqiString = EXCELLENT;
        break;
      case 1:
        AQI.AqiString = EXCELLENT;
        break;           
      }
  } else if ( COUNTRY == 1 ) {
    // European CAQI
    switch ( AQI.AQI) {
      case 25: 
        AQI.AqiString = GOOD;
        break;
      case 50:
        AQI.AqiString = ACCEPTABLE;
        break;
      case 75:
        AQI.AqiString = MODERATE;
        break;
      case 100:
        AQI.AqiString = HEAVY;
        break;         
      default:
        AQI.AqiString = SEVERE;
      }  
  } else if ( COUNTRY == 2 ) {
    // USA / CN
    if ( AQI.AQI <= 50 ) {
        AQI.AqiString = GOOD;
    } else if ( AQI.AQI > 50 && AQI.AQI <= 100 ) {
        AQI.AqiString = ACCEPTABLE;
    } else if ( AQI.AQI > 100 && AQI.AQI <= 150 ) {
        AQI.AqiString = MODERATE;
    } else if ( AQI.AQI > 150 && AQI.AQI <= 200 ) {
        AQI.AqiString = HEAVY;
    } else if ( AQI.AQI > 200 && AQI.AQI <= 300 ) {  
        AQI.AqiString = SEVERE;
    } else {    
       AQI.AqiString = HAZARDOUS;
    }  
  }
}
/*
 * CAQI Européen - European CAQI level 
 * source : http://www.airqualitynow.eu/about_indices_definition.php
 */
 
int getACQI( int sensor, float density ){  
  if ( sensor == 0 ) {  //PM2,5
    if ( density == 0 ) {
      return 0; 
    } else if ( density <= 15 ) {
      return 25 ;
    } else if ( density > 15 && density <= 30 ) {
      return 50;
    } else if ( density > 30 && density <= 55 ) {
      return 75;
    } else if ( density > 55 && density <= 110 ) {
      return 100;
    } else {
      return 150;
    }
  } else {              //PM10
    if ( density == 0 ) {
      return 0; 
    } else if ( density <= 25 ) {
      return 25 ;
    } else if ( density > 25 && density <= 50 ) {
      return 50;
    } else if ( density > 50 && density <= 90 ) {
      return 75;
    } else if ( density > 90 && density <= 180 ) {
      return 100;
    } else {
      return 150;
    }
  }
}

/*
 * AQI formula: https://en.wikipedia.org/wiki/Air_Quality_Index#United_States
 * Arduino code https://gist.github.com/nfjinjing/8d63012c18feea3ed04e
 * On line AQI calculator https://www.airnow.gov/index.cfm?action=resources.conc_aqi_calc
 */
float calcAQI(float I_high, float I_low, float C_high, float C_low, float C) {
  return (I_high - I_low) * (C - C_low) / (C_high - C_low) + I_low;
}

int getAQI(int sensor, float density) {
  int d10 = (int)(density * 10);
  if ( sensor == 0 ) {
    if (d10 <= 0) {
      return 0;
    }
    else if(d10 <= 120) {
      return calcAQI(50, 0, 120, 0, d10);
    }
    else if (d10 <= 354) {
      return calcAQI(100, 51, 354, 121, d10);
    }
    else if (d10 <= 554) {
      return calcAQI(150, 101, 554, 355, d10);
    }
    else if (d10 <= 1504) {
      return calcAQI(200, 151, 1504, 555, d10);
    }
    else if (d10 <= 2504) {
      return calcAQI(300, 201, 2504, 1505, d10);
    }
    else if (d10 <= 3504) {
      return calcAQI(400, 301, 3504, 2505, d10);
    }
    else if (d10 <= 5004) {
      return calcAQI(500, 401, 5004, 3505, d10);
    }
    else if (d10 <= 10000) {
      return calcAQI(1000, 501, 10000, 5005, d10);
    }
    else {
      return 1001;
    }
  } else {
    if (d10 <= 0) {
      return 0;
    }
    else if(d10 <= 540) {
      return calcAQI(50, 0, 540, 0, d10);
    }
    else if (d10 <= 1540) {
      return calcAQI(100, 51, 1540, 541, d10);
    }
    else if (d10 <= 2540) {
      return calcAQI(150, 101, 2540, 1541, d10);
    }
    else if (d10 <= 3550) {
      return calcAQI(200, 151, 3550, 2541, d10);
    }
    else if (d10 <= 4250) {
      return calcAQI(300, 201, 4250, 3551, d10);
    }
    else if (d10 <= 5050) {
      return calcAQI(400, 301, 5050, 4251, d10);
    }
    else if (d10 <= 6050) {
      return calcAQI(500, 401, 6050, 5051, d10);
    }
    else {
      return 1001;
    }
  }   
}
