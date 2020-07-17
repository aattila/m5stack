
#include <EEPROM.h>
#include <M5Stack.h>
#include <WiFi.h>
#include <Wire.h>
#include "PCF8574.h" 
#include "DHT12.h"

#include "Adafruit_SGP30.h"
#include "Adafruit_Sensor.h"
#include <Adafruit_BMP280.h>

#define DEVICE_ID   "001"

// I2C
#define PCF_ADDR    0x20   //PCF8574 address
#define DHT_ADDR    0x5c   //Env module's DHT sensor address
#define BMP_ADDR    0x76   //Env module's BMP280 sensor address 
#define GAS_ADDR    0x58   //Adafruit SGP30 VOC and eCO2 sensor

// SGP30 - https://learn.adafruit.com/adafruit-sgp30-gas-tvoc-eco2-mox-sensor/arduino-code

// PCF8574
#define PCFINT_PIN  2       //GPIO2  PCF8574 interrupt pin
#define PUMP_PIN P0        //PCF pressure switch 
#define WATER_COUNT_PIN P1  //PCF consumed water volume counter 0.5 liter/pulse
#define POWER_PULSE_PIN P2  //PCF power consumption 1000 pulse/kWh (1 imp/Watt)
#define FLOOD_PIN P3        //PCF flood sensing

#define PUMP_RELAY_PIN P6   //PCF pump relay 
#define VENT_RELAY_PIN P7   //PCF vent relay

// RS485
#define RX_PIN 16  //GPIO16 RS485 RX
#define TX_PIN 17  //GPIO17 RS485 TX

// others
#define X_LOCAL 40
#define Y_LOCAL 40

#define X_OFF 160
#define Y_OFF 30

DHT12 dht12;
Adafruit_BMP280 bme;
Adafruit_SGP30 sgp;
PCF8574 pcf8574(PCF_ADDR);

hw_timer_t * timer = NULL;

unsigned long lastEnvRead, lastBaselineRead, lastDisplay;
unsigned int displayIdx;

volatile bool pcfEvent, timerEvent;
unsigned long waterCounter, powerPulse;
bool waterCounterArmed, powerPulseArmed;
bool pumpSwitch;
bool dataChanged;
bool isAlarm, isBeep = true, tempAlarm, floodAlarm, gasAlarm, isVent, isRS485;

float tmp, hum, pressure;
int tvoc, eco2;

int EEPROM_SIZE = 2*sizeof(long);
int eeAddress1 = 0;
int eeAddress2 = eeAddress1 + sizeof(long) + 1;

uint8_t music[1000];
int alertColor = TFT_RED;
int tempColor = TFT_GREEN;
int humiColor = TFT_BLUE;
int pressColor = TFT_LIGHTGREY;


portMUX_TYPE pcfMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  timerEvent = true;
  portEXIT_CRITICAL_ISR(&timerMux);
 
}

void IRAM_ATTR pcfISR() {
  portENTER_CRITICAL(&pcfMux);
  pcfEvent = true;
  portEXIT_CRITICAL(&pcfMux);
}


void drawButton(uint8_t x, uint8_t y, bool isEnabled) {
  uint8_t rad = 9;
  uint8_t w = 45;
  uint8_t pad = 2;
  if(isEnabled) {
    M5.Lcd.fillRoundRect(x, y, w, 2*rad, rad, TFT_GREEN);
    M5.Lcd.fillCircle(x+w-rad-pad, y+rad, rad+pad, TFT_GREEN);
    M5.Lcd.drawCircle(x+w-rad-pad, y+rad, rad, TFT_LIGHTGREY);
    M5.Lcd.fillCircle(x+w-rad-pad, y+rad, rad-pad, TFT_WHITE);
  } else {
    M5.Lcd.fillRoundRect(x, y, w, 2*rad, rad, TFT_DARKGREY);
    M5.Lcd.fillCircle(x+rad+pad, y+rad, rad+pad, TFT_DARKGREY);
    M5.Lcd.drawCircle(x+rad+pad, y+rad, rad, TFT_LIGHTGREY);
    M5.Lcd.fillCircle(x+rad+pad, y+rad, rad-pad, TFT_WHITE);
  }
}


void reportData() {
  uint8_t alarm = isAlarm?1:0;
  uint8_t flood = floodAlarm?1:0;
  uint8_t pump = pumpSwitch?1:0;
  float water = (float)waterCounter/10;
  float power = (float)powerPulse/1000;

  char buff[250];
  sprintf(buff, "%s: { \"temperature\": %2.2f,  \"humidity\": %3.2f%, \"pressure\": %4.1f, \"alarm\": %1u, \"waterCounter\": %0.1f, \"powerCounter\": %0.3f, \"flood\": %1u, \"pump\": %1u, \"tVOC\": %u, \"eCO2\": %u }", DEVICE_ID, tmp, hum, pressure, alarm, water, power, flood, pump, tvoc, eco2);
  Serial.println(buff);
}

void displayMenu() {
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.drawFastHLine(0, 210, 320, TFT_DARKGREY);
  M5.Lcd.setTextSize(1);

  if(isVent) {
    M5.Lcd.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else {
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  }
  M5.Lcd.drawString(" Vent ", 35, 215, 4);

  if(isAlarm) {
    if(isBeep) {
      M5.Lcd.setTextColor(alertColor, TFT_BLACK);    
    } else {
      M5.Lcd.setTextColor(TFT_LIGHTGREY, alertColor);    
    }
  } else {
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  }
  M5.Lcd.drawString(" Alarm ", 123, 215, 4);

  if(isRS485) {
    M5.Lcd.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else {
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  }
  M5.Lcd.drawString(" RS485 ", 213, 215, 4);
  
}

void displayEnv() {
  M5.Lcd.clear(BLACK);
  
  String temp = String(tmp, 1);
  String humi = String(hum, 1);
  String pres = String(pressure, 1);

  
  M5.Lcd.setTextDatum(TL_DATUM);
  if(tempAlarm) {
    M5.Lcd.setTextColor(alertColor, TFT_BLACK);
  } else {
    M5.Lcd.setTextColor(tempColor, TFT_BLACK);    
  }
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString(temp, 60, 0, 7);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString("C", 280, 0, 4);

  M5.Lcd.setTextColor(humiColor, TFT_BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString(humi, 60, 107, 7);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString("%", 280, 107, 4);

  M5.Lcd.setRotation(0);
  M5.Lcd.setTextColor(pressColor, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString(pres, 40, 0, 7);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString("hPa", 190, 0, 4);
  M5.Lcd.setRotation(1);

  displayMenu();

}

void displaySGP() {
  M5.Lcd.clear(BLACK);

  int voffset = 0;

  String _tvoc = String(tvoc);
  String _eco2 = String(eco2);

  M5.Lcd.setTextDatum(TL_DATUM);
  if(gasAlarm) {
    M5.Lcd.setTextColor(alertColor, TFT_BLACK);
  } else {
    M5.Lcd.setTextColor(TFT_ORANGE, TFT_BLACK);    
  }
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString(_tvoc, 80, voffset, 7);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Lcd.drawString("TVOC", 0, voffset, 4);
  M5.Lcd.drawString("ppb", 17, voffset+25, 4);

  voffset = voffset+80;
  
  if(gasAlarm) {
    M5.Lcd.setTextColor(alertColor, TFT_BLACK);
  } else {
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  }
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString(_eco2, 80, voffset, 7);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Lcd.drawString("eCO2", 0, voffset, 4);
  M5.Lcd.drawString("ppm", 13, voffset+25, 4);


  displayMenu();
}

void displayQuant() {
  M5.Lcd.clear(BLACK);

  int voffset = 0;

  String water = String((float)waterCounter/10, 1); // 100000.0
  String power = String((float)powerPulse/1000, 3); // 2000.000

  int maxLen = 8;
  for(int i=water.length(); i<maxLen; i++) {
    water = "0" + water;
  }
  for(int i=power.length(); i<maxLen; i++) {
    power = "0" + power;
  }

  M5.Lcd.setTextDatum(TL_DATUM);

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Lcd.drawString(water, 70, voffset, 7);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Lcd.drawString("liter", 0, voffset, 4);

  voffset = voffset+80;

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_DARKCYAN, TFT_BLACK);
  M5.Lcd.drawString(power, 70, voffset, 7);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Lcd.drawString("kWh", 0, voffset, 4);

  displayMenu();
}


void setup() {
  
  EEPROM.begin(EEPROM_SIZE);
  
  // all radio switched off
  WiFi.mode(WIFI_OFF);
  btStop();

  // timer for regular raporting the env data
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
//  timerAlarmWrite(timer, 15*60*1000000, true); // 15 minutes
  timerAlarmWrite(timer, 60*1000000, true); // 1 minutes
  timerAlarmEnable(timer);

  M5.begin();
  M5.Power.begin();
  Wire.begin();
  
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Starting...");


  while (!bme.begin(BMP_ADDR)){  
     M5.Lcd.println("Could not find a valid BMP280 sensor, check wiring!");
  }
    

  // debug init
  Serial.begin(115200);
  // RS485 init
  M5.Lcd.println("Begin RS485");
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);  

  // SGP30 init
  if (!sgp.begin()){
    Serial.println("Adafruit SGP30 Sensor not found!");
  } else {
    Serial.print("Found SGP30 serial #");
    Serial.print(sgp.serialnumber[0], HEX);
    Serial.print(sgp.serialnumber[1], HEX);
    Serial.println(sgp.serialnumber[2], HEX);
    
    sgp.setIAQBaseline(0x88CE, 0x8B1C); 

  }


  // setup PCF interrupt
  pinMode(PCFINT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PCFINT_PIN), pcfISR, FALLING);

  // I/O init
  M5.Lcd.println("Begin PCF8574");
  pcf8574.pinMode(PUMP_PIN, INPUT);
  pcf8574.pinMode(WATER_COUNT_PIN, INPUT);
  pcf8574.pinMode(POWER_PULSE_PIN, INPUT);
  pcf8574.pinMode(FLOOD_PIN, INPUT);
  pcf8574.pinMode(PUMP_RELAY_PIN, OUTPUT);
  pcf8574.pinMode(VENT_RELAY_PIN, OUTPUT);
  pcf8574.begin();

  M5.Lcd.clear(BLACK);

//  drawButton(255, 100, true);
//  drawButton(255, 130, false);


  waterCounter = EEPROMReadlong(eeAddress1);
  // apply corrections to get .5 or 0 at the ending of the number
  String wc1 = String(waterCounter);
  if(wc1.endsWith("0") or wc1.endsWith("5") ) {
    // do nothing
  } else {
    int wc2 = (int) waterCounter/10;
    waterCounter = wc2*10;
  }
  
  powerPulse = EEPROMReadlong(eeAddress2);

}

void loop() {
  
//  Serial2.write("abcd\n");

  M5.update();
  if (M5.BtnA.wasReleased()) {
    isVent = !isVent;
    pcf8574.digitalWrite(VENT_RELAY_PIN, HIGH);
    displayMenu();
  }
  if (M5.BtnB.wasReleased()) {
    isBeep = !isBeep;
    displayMenu();
  }
  if (M5.BtnC.wasReleased()) {
    isRS485 = !isRS485;
    displayMenu();
  }

  if (millis() > lastEnvRead + 1000) {
    readEnv();
    sgp.setHumidity(getAbsoluteHumidity(tmp, hum));

    if (!sgp.IAQmeasure()) {
      Serial.println("SGP30 Measurement failed");
    } else {
      tvoc = sgp.TVOC;
      eco2 = sgp.eCO2;
    }

    tempAlarm = tmp < 0 or tmp > 20;
    gasAlarm = tvoc > 50 or eco2 > 500;

    isAlarm = tempAlarm or floodAlarm or gasAlarm;
    // reset the silenced beeps
    if(!isAlarm) {
      isBeep = true;
    }
    if(isAlarm and isBeep) {
      beep(100);
    }

    lastEnvRead = millis();
  }
  
  if(millis() > lastBaselineRead + 30000) {
    readSGPBaseline();
    lastBaselineRead = millis();
  }


  if(millis() > lastDisplay + 10000) {
    if(displayIdx == 0) {
      displayEnv();
    }
    if(displayIdx == 1) {
      displaySGP();
    }
    if(displayIdx == 2) {
      displayQuant();
    }
    
    displayIdx++;
    if(displayIdx>2) {
      displayIdx = 0;
    }
    
    lastDisplay = millis();
  }

 
  if (pcfEvent) {
    
    bool pumpSwitchPre = pumpSwitch;
    bool floodAlarmPre = floodAlarm;
    dataChanged = false;

    PCF8574::DigitalInput val = pcf8574.digitalReadAll();    
    if (val.p0==HIGH) pumpSwitch = true; else pumpSwitch = false;
    if (val.p1==HIGH) waterCounterArmed = true; else {
      if(waterCounterArmed) {
        waterCounterArmed = false;
        waterCounter = waterCounter+5;
        EEPROMWritelong(eeAddress1, waterCounter);
        reportData();
      }
    }
    if (val.p2==HIGH) powerPulseArmed = true; else {
      if(powerPulseArmed) {
        powerPulseArmed = false;
        powerPulse++;
        EEPROMWritelong(eeAddress2, powerPulse);
        reportData();
      }
    }
    if (val.p3==HIGH) floodAlarm = true; else floodAlarm = false;

    if(pumpSwitch != pumpSwitchPre) {
      reportData();
    }

    if(floodAlarm != floodAlarmPre) {
      reportData();
    }

    pcfEvent= false;
  }  

  if(timerEvent) {
    timerEvent = false;
    reportData();
  }
}

void tone_volume(uint16_t frequency, uint32_t duration) {
  float interval=0.001257 * float(frequency);
  float phase=0;
  for (int i=0;i<1000;i++) {
    music[i]=127+126 * sin(phase);
    phase+=interval;
  }
  music[999]=0;
  int remains=duration;
  for (int i=0;i<duration;i+=200) {
    if (remains<200) {
      music[remains*999/200]=0;
    }
    M5.Speaker.playMusic(music, 5000);
    remains -= 200;
  }
}

void beep(int leng) {
  M5.Speaker.setVolume(1);
  tone_volume(1000, leng);
}

void readEnv() {
  tmp = dht12.readTemperature();
  hum = dht12.readHumidity();
  pressure = bme.readPressure()/100;
}

void readSGPBaseline() {
  uint16_t TVOC_base, eCO2_base;
  if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
    Serial.println("Failed to get baseline readings");
    return;
  }
  Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
  Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
}

long EEPROMReadlong(int address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);
 
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void EEPROMWritelong(int address, long value) {
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);
 
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
  EEPROM.commit();
}

/* return absolute humidity [mg/m^3] with approximation formula
* @param temperature [°C]
* @param humidity [%RH]
*/
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}
