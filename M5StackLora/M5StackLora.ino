#define M5STACK_MPU6886 

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <M5Stack.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include "DHT12.h"
#include "Adafruit_Sensor.h"
#include <Adafruit_BMP280.h>

extern "C" int rom_phy_get_vdd33();

// I2C
#define DHT_ADDR    0x5c   //Env module's DHT sensor address
#define BMP_ADDR    0x76   //Env module's BMP280 sensor address 

DHT12 dht12;
Adafruit_BMP280 bme;

float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;

float gyroX = 0.0F;
float gyroY = 0.0F;
float gyroZ = 0.0F;

float pitch = 0.0F;
float roll  = 0.0F;
float yaw   = 0.0F;

float   lat = 0.0F; 
float   lon = 0.0F; 
int16_t alt = 0;
float   spd = 0.0F;

uint8_t charge = 0;
uint8_t isCharging = false;
uint8_t hum  = 0;
float   temp = 0.0F;
float   pres = 0.0F;

static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
HardwareSerial ss(2);


bool isForce, isSend;
unsigned long lastEnvRead;

// LSB
static const u1_t PROGMEM APPEUI[8] = { 0x11, 0x08, 0x33, 0xF4, 0xFF, 0xA8, 0xC5, 0x60 };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// LSB
static const u1_t PROGMEM DEVEUI[8] = { 0x96, 0x5A, 0xF4, 0xD9, 0x7A, 0x31, 0x11, 0x00 };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// MSB
static const u1_t PROGMEM APPKEY[16] = { 0x85, 0x2B, 0xE0, 0x93, 0x34, 0xC5, 0x9D, 0x44, 0x60, 0xCC, 0x2A, 0xDA, 0x7D, 0x4F, 0xBC, 0x8B };
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

uint8_t payload[27] = {};

static osjob_t sendjob;

String txIntervalLabel[] = {" Loop  ", "  10s   ", "  30s   ", "1min  ", "3min  ", "5min   ", "10min  ", "15min  ", "30min  ", "    1h    "};
uint16_t txInterval[] = {0, 10, 30, 60, 180, 300, 600, 900, 1800, 3600};
unsigned txIntervalIdx = 0;


// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 5,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 26,
    .dio = {36},
};


void onEvent (ev_t ev) {
      
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            status("Scan timeout", true);
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            status("Joining to gateway", false);
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            status("Join SUCCESS", false);

            // Disable link check validation (automatically enabled
            // during join, but not supported by TTN at this time).
            //LMIC_setLinkCheckMode(0);
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            status("Join FAILED", true);
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            status("Re-Join FAILED", true);
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            status("Tx DONE", false);
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            if(isSend and txIntervalIdx > 0) {
              // Schedule next transmission
              os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(txInterval[txIntervalIdx]), do_send);
             } else {
               isSend = false;
               displayMenu();
             }
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            status("Lost TSYNC", true);
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            status("Reset", false);
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            status("Rx DONE", false);
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            status("Link DEAD", true);
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            status("Link ALIVE", false);
            break;
         default:
            Serial.println(F("Unknown event"));
            status("Unknown Event", true);
            break;
    }
}

void do_send(osjob_t* j) {
    
  if(isForce) {
    // this will disable the duty cycle check
    LMIC.bands[BAND_MILLI].avail =
    LMIC.bands[BAND_CENTI].avail =
    LMIC.bands[BAND_DECI ].avail = os_getTime();
  }

  
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
     Serial.println(F("OP_TXRXPEND, not sending"));
     status("Tx FAILED - pending prev Tx", true);
  } 
  // Prepare upstream data transmission at the next possible time.
  LMIC_setTxData2(1, payload, sizeof(payload), 0);
  Serial.println(F("Packet queued"));
  status("Prepared for Tx", false);

}

void setup() {

  M5.begin();
  M5.Power.begin();

  M5.IMU.Init();

  ss.begin(GPSBaud);
  M5.Lcd.setTextSize(1);
  displayMenu();

  while (!bme.begin(BMP_ADDR)){  
    M5.Lcd.println("Could not find a valid BMP280 sensor, check wiring!");
  }

  os_init();
  LMIC_reset();
  LMIC_startJoining();

}

void loop() {
  M5.update();
  if (M5.BtnA.wasReleased()) {
    isForce = !isForce;
    displayMenu();
  }
  if (M5.BtnB.wasReleased()) {
    isSend = !isSend;
    M5.Lcd.clear(BLACK);
    displayMenu();
    preaparePayload();
    do_send(&sendjob);
  }
  if (M5.BtnC.wasReleased()) {
    txIntervalIdx ++;
    if(txIntervalIdx >= 10) {
      txIntervalIdx = 0;
    }
    displayMenu();
  }

  
  if (millis() > lastEnvRead + 2000) {
    // read GPS
    readGPS();
    // read environment
    readEnv();
    // read accelerometer
    readAccel();

    displayData();
    
    lastEnvRead = millis(); 
  }

  os_runloop_once();
}

void displayMenu() {
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.drawFastHLine(0, 30,  320, TFT_DARKGREY);
  M5.Lcd.drawFastHLine(0, 210, 320, TFT_DARKGREY);
  M5.Lcd.setTextSize(1);

  if(isForce) {
    M5.Lcd.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else {
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  }
  M5.Lcd.drawString(" Force ", 31, 215, 4);

  if(isSend) {
    M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);    
    if(txIntervalIdx > 0) {
      M5.Lcd.drawString(" Stop  ", 128, 215, 4);    
    }
  } else {
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    if(txIntervalIdx > 0) {
      M5.Lcd.drawString(" Start  ", 128, 215, 4);    
    }
  }
  if(txIntervalIdx == 0) {
    M5.Lcd.drawString(" Send ", 128, 215, 4);
  }

  if(txIntervalIdx > 0) {
    M5.Lcd.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else {
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  }
  M5.Lcd.drawString(txIntervalLabel[txIntervalIdx], 218, 215, 4);
  
}


void preaparePayload() {

  int16_t  lat1;
  uint32_t lat2;
  char buffer1[50];
  sprintf(buffer1, "%lf", lat);
  sscanf(buffer1, "%d.%d", &lat1, &lat2);

  int16_t  lon1;
  uint32_t lon2;
  char buffer2[50];
  sprintf(buffer2, "%lf", lon);
  sscanf(buffer2, "%d.%d", &lon1, &lon2);

  int16_t  t = temp*10;
  uint16_t p = pres*10;
 
  // senzor data to LORA payload
  payload[0] = isCharging;
  payload[1] = charge;
  payload[2] = hum;
  payload[3] = t >> 8;
  payload[4] = t;
  payload[5] = p >> 8;
  payload[6] = p;

  payload[7]  = lat1 >> 8;
  payload[8]  = lat1;
  payload[9]  = lat2 >> 24;
  payload[10] = lat2 >> 16;
  payload[11] = lat2 >> 8;
  payload[12] = lat2;      
  
  payload[13] = lon1 >> 8;
  payload[14] = lon1;
  payload[15] = lon2 >> 24;
  payload[16] = lon2 >> 16;
  payload[17] = lon2 >> 8;
  payload[18] = lon2;
  
  payload[19] = alt >> 8;
  payload[20] = alt;

  int16_t x = gyroX;
  int16_t y = gyroY;
  int16_t z = gyroZ;

  payload[21] = x >> 8;
  payload[22] = x;
  payload[23] = y >> 8;
  payload[24] = y;
  payload[25] = z >> 8;
  payload[26] = z;

}


void displayData() {
  M5.Lcd.setCursor(0, 43);
  M5.Lcd.setTextSize(2);

  M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
  println("GPS: "+String(lat, 6), 5);
  println("     "+String(lon, 6), 5);
  println("     "+String(alt), 5);
  
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_BLUE, TFT_BLACK);  
  println("GYRO: "+String(pitch, 2)+" "+String(roll, 2)+" "+String(yaw, 2), 13);
//  Serial.printf("%6.2f  %6.2f  %6.2f       o/s\n", gyroX, gyroY, gyroZ);
//  Serial.printf(" %5.2f   %5.2f   %5.2f    G\n", accX, accY, accZ);
//  Serial.printf(" %5.2f   %5.2f   %5.2f    deg\n", pitch, roll, yaw);

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_DARKCYAN, TFT_BLACK);  
  println("Lvl: "+String(charge)+"% P: "+String(pres, 2)+"hPa", 13);
  println("Hum: "+String(hum)+"% Temp: "+String(temp, 2)+"C", 13);

}

void status(String msg, bool isError) {
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextSize(1);
  if(isError) {
    M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);  
  } else {
    M5.Lcd.setTextColor(TFT_GREENYELLOW, TFT_BLACK);  
  }
  M5.Lcd.fillRect(0, 0, 320, 29, TFT_BLACK);
  M5.Lcd.drawString(msg, 0, 0, 4);
}

void println(String msg, unsigned padding) {
  int y = M5.Lcd.getCursorY();
  M5.Lcd.setCursor(0, y+padding);
  M5.Lcd.println(msg);
}

void readGPS() {
  while (ss.available()) {
    gps.encode(ss.read());
  }
  if(gps.location.isValid()) {
    lat = gps.location.lat();
    lon = gps.location.lng();
  }
  if(gps.altitude.isValid()) {
    alt = gps.altitude.meters();
  }
  if(gps.speed.isValid()) {
    spd = gps.speed.kmph();
  }
}

void readAccel() {
  M5.IMU.getGyroData(&gyroX,&gyroY,&gyroZ);
  M5.IMU.getAccelData(&accX,&accY,&accZ);
  M5.IMU.getAhrsData(&pitch,&roll,&yaw);
}

void readEnv() {
  isCharging = M5.Power.isCharging();
  charge = M5.Power.getBatteryLevel();
  temp = dht12.readTemperature();
  hum  = dht12.readHumidity();
  float p = bme.readPressure()/100;

  if(p > 1100 or p < 800) {
//    Serial.print("Bad pressure reading: ");
//    Serial.println(p);
  } else {
    pres = p;
  }
}
