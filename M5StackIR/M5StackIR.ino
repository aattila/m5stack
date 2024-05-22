#include "credentials.h"

#include <M5Stack.h>
#include "Free_Fonts.h" 

#include <WM8978.h> /* https://github.com/CelliesProjects/wm8978-esp32 */
#include <Audio.h>  /* https://github.com/schreibfaul1/ESP32-audioI2S */

#include <NTPClient.h> /* https://github.com/arduino-libraries/NTPClient */
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"0.pool.ntp.org", 3*3600, 60000);

#include "DHT12.h"
DHT12 DHT;

#include <Adafruit_NeoPixel.h>
const uint16_t PixelCount = 12;
const uint8_t PixelPin = 15;
Adafruit_NeoPixel pixels(PixelCount, PixelPin, NEO_GRB + NEO_KHZ800);

/* M5Stack Node WM8978 I2C pins */
#define I2C_SDA     21
#define I2C_SCL     22

/* M5Stack Node I2S pins */
#define I2S_BCK      5
#define I2S_WS      13
#define I2S_DOUT     2
#define I2S_DIN     34

/* M5Stack WM8978 MCLK gpio number */
#define I2S_MCLKPIN  0

WM8978 dac;
Audio audio;

bool isPlaying = true;
bool isSpkMode = false;
bool isLongPress = false;
bool isWifi = false;
bool isSDCard = false;

unsigned int scrollSize = 4;
int spkVolume = 30; /* max 63 */
int hpVolume[] = {20, 20};
int _spkVolume;
int _hpVolume[2];
unsigned long lastEnvRead;
unsigned long saveCheck;
unsigned long ntpUpdate;

unsigned int red   = 255; 
unsigned int green = 109;
unsigned int blue  = 10;

const char* fileState = "/config.txt";
const char* fileCfg = "/playlist.txt";
String cfg = "";

typedef struct station {
  String name;
  String url;
} Station;

unsigned int _stationIdx = 0;
unsigned int stationIdx = 0;
unsigned int stationsSize = 5;

Station stations[500];


Station bkpStations[] = {
//  {"Paprika Radio", "http://stream1.paprikaradio.ro:8000/;stream.nsv"},
//  {"Marosvasarhelyi Radio", "http://streaming.radiomures.ro:8312/;stream.nsv&type=mp3"},
//  {"Tilos Radio", "http://stream.tilos.hu:8000/tilos"},
  {"Hirschmilch Chillout", "http://hirschmilch.de:7000/chillout.mp3"},
  {"Antenne Bayern Chillout", "http://mp3channels.webradio.antenne.de/chillout?type=.mp3"},
  {"Radio Gaia - Chill Out", "http://streamingV2.shoutcast.com/Chill-Out-Radio-Gaia?lang=en-GB%2cen-US%3bq%3d0.9%2cen%3bq%3d0.8"},
  {"Psychedelik - Ambient", "https://stream.psychedelik.com:8002/"},
  {"Psychedelik - PsyTrance", "https://stream.psychedelik.com:8000/"}
};


void setup() {

  ntpUpdate = millis();
 
  Serial.begin(115200);
  Wire.begin();
  DHT.begin();

  M5.begin();
  M5.Lcd.setBrightness(20);
  M5.Power.begin();
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);  
  M5.Lcd.setRotation(3);

  delay(500);
  
  M5.Lcd.drawString("DAC: ", 5, 10, 4);
  M5.Lcd.drawString("Success", 70, 10, 4);

  delay(1000);
  
  M5.Lcd.drawString("   SD: ", 5, 40, 4);
  isSDCard = readStations(SD, fileCfg);
  if(isSDCard) {
    M5.Lcd.drawString("Yes", 70, 40, 4);
    readState();
  } else {
    M5.Lcd.drawString("No", 70, 40, 4);
    for(int i=0; i<stationsSize; i++) {
      stations[i] = bkpStations[i];
    }
  }

  pixels.begin();
  pixels.clear();
  for(int i=0; i<PixelCount; i++) {
    pixels.setPixelColor(i, pixels.Color(red, green, blue));
    pixels.show();
  }

  delay(500);
  
  M5.Lcd.drawString("WiFi: Connecting", 5, 70, 4);
  if(isSDCard) {
    String ssid = getValue(cfg, ',', 0, true);
    char ssidBuf[ssid.length()+1];
    ssid.toCharArray(ssidBuf, sizeof(ssidBuf));
    String pass = getValue(cfg, ',', 1, true);
    char passBuf[pass.length()+1];
    pass.toCharArray(passBuf, sizeof(passBuf));
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssidBuf);
    WiFi.begin(ssidBuf, passBuf);
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  delay(500);
  for(int i=0; i<200; i++) {
    // 20 sec to try connecting to the WiFi
    if (WiFi.isConnected()) {
      isWifi = true;  
      break;   
    } else {
      delay(100);
    }
  }

  M5.Lcd.fillRect(80, 40, 240, 30, TFT_BLACK);
  if(isWifi) {
    M5.Lcd.drawString("Succes", 70, 70, 4);
  } else {
    M5.Lcd.drawString("Failed", 70, 70, 4);    
  }
  isPlaying = isWifi;

  timeClient.begin();

  /* Setup wm8978 I2C interface */
  if (!dac.begin(I2C_SDA, I2C_SCL)) {
    log_e("Error setting up dac. System halted");
    while (1) delay(100);
  }
  dac.setSPKvol(spkVolume);
  dac.setHPvol(hpVolume[0], hpVolume[1]);

  /* Setup wm8978 I2S interface */
  audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCLKPIN);

  displayList();
  displayHeaders();
  playStation();
}

void loop() {

  if (M5.BtnC.wasPressed()) {
    Serial.print("C pressed");
    ntpUpdate = millis();
    if(isPlaying) {
      decVolume();
    } else {
      prevStation();
    }
  } 
  
  if(M5.BtnB.pressedFor(2000)) {
    if(!isLongPress) {
      isSpkMode = !isSpkMode;
      displayHeaders();
      writeState();
      isLongPress = true;
    }
    M5.update();
  } else if (M5.BtnB.wasReleased()) {
      ntpUpdate = millis();
      if(isLongPress) {
        isLongPress = false;  
      } else {
        isPlaying = !isPlaying;
        audio.pauseResume();
        if(isPlaying and _stationIdx != stationIdx) {
          writeState();
          audio.stopSong();
          playStation();
        }
        displayList();
        displayHeaders();
      }
  }

  if (M5.BtnA.wasPressed()) {
    Serial.print("A pressed");
    ntpUpdate = millis();
    if(isPlaying) {
      incVolume();
    } else {
      nextStation();
    }
  }
 
  audio.loop(); 

  M5.update();

  if(audio.isRunning()) {
    // re-display the status info, especially for the buffer monitoring
    if (millis() > lastEnvRead + 1000) {
      lastEnvRead = millis(); 
      displayInfo();
    }
  } else {
    if (millis() > ntpUpdate + 30000) {
      showTime();
      ntpUpdate = millis(); 
      readDHT();
    }
    delay(200);
  }   

  // saving volume state
  if (millis() > saveCheck + 30000) {
    if(_spkVolume != spkVolume or _hpVolume[0] != hpVolume[0] or _hpVolume[1] != hpVolume[1]) {
      writeState();
    }
    saveCheck = millis(); 
  }
}

void showTime() {
  Serial.print("Time from NTP: ");
  timeClient.update();
  String ftime = timeClient.getFormattedTime();
  Serial.println(ftime);

  M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);    
  if(isPlaying) {
    M5.Lcd.fillRect(0, 25, 320, 215, TFT_BLACK);
    M5.Lcd.setTextDatum(CC_DATUM);
    M5.Lcd.drawString(ftime.substring(0,5), 160, 90+25, 8);
    M5.Lcd.setTextDatum(BC_DATUM);
    M5.Lcd.setFreeFont(FSS12);
    M5.Lcd.drawString(stations[stationIdx].name, 160, 230, GFXFF);
  } else {
    M5.Lcd.fillRect(0, 0, 320, 240, TFT_BLACK);
    M5.Lcd.setTextDatum(CC_DATUM);
    M5.Lcd.drawString(ftime.substring(0,5), 160, 120, 8);
  }
  M5.Lcd.setTextDatum(TL_DATUM);
}

void playStation() {
  String url = stations[stationIdx].url;
  int str_len = url.length() + 1; 
  char char_array[str_len];
  url.toCharArray(char_array, str_len);
  audio.connecttohost(char_array);
}

void displayList() {
  M5.Lcd.fillRect(0, 25, 320, 188, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS18);
  int size = scrollSize;
  if(stationsSize < scrollSize) {
    size = stationsSize;
  }
  int scrollOffset = 0;
  if(stationIdx>=scrollSize) {
    scrollOffset = stationIdx-scrollSize+1;
  }
  
  for(int i=scrollOffset; i < (scrollOffset+size); i++) {
    if(i == stationIdx) {
      M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);    
    } else if (i == (stationIdx-1) or i == (stationIdx+1)) {
      M5.Lcd.setTextColor(M5.Lcd.color565(130,130,130), TFT_BLACK);
    } else if (i == (stationIdx-2) or i == (stationIdx+2)) {
      M5.Lcd.setTextColor(M5.Lcd.color565(100,100,100), TFT_BLACK);
    } else {
      M5.Lcd.setTextColor(M5.Lcd.color565(70,70,70), TFT_BLACK);
    }
    
    M5.Lcd.drawString(stations[i].name, 0, 44+(i-scrollOffset)*42, GFXFF);
  }

}

void displayInfo() {
  M5.Lcd.setTextColor(TFT_DARKGREY, M5.Lcd.color565(50,50,50));

  if(isWifi) {
    M5.Lcd.drawString("Wifi", 5, 4, 2);
  }

  if(isSDCard) {
    M5.Lcd.drawString("SD", 40, 4, 2);
  }

  M5.Lcd.fillRect(100, 5, 120, 15, TFT_BLACK);
  // max buff is 6399 bytes
  // max buff in pixels will be 120
  int buffStatus = audio.inBufferFilled()*120/6399;
  M5.Lcd.fillRect(101, 6, buffStatus, 13, M5.Lcd.color565(100,100,100));

  int kbps = audio.getBitRate()/1000;
  M5.Lcd.drawString(String(kbps)+" kbps", 260, 4, 2);
}

void prevStation() {
  if(stationIdx > 0) {
    stationIdx = stationIdx - 1;
    displayList();
  }
}

void nextStation() {
  if(stationIdx < (stationsSize-1)) {
    stationIdx = stationIdx + 1;
    displayList();
  }
}

void decVolume() {
  if(isSpkMode) {
    spkVolume = spkVolume - 2;
    if(spkVolume < 0) {
      spkVolume = 0;
    }
  } else {
    hpVolume[0] = hpVolume[0] - 2;
    hpVolume[1] = hpVolume[1] - 2;
    if(hpVolume[0] < 0) {
      hpVolume[0] = 0;
    }
    if(hpVolume[1] < 0) {
      hpVolume[1] = 0;
    }
  }
  displayVolume();
}

void incVolume() {
  if(isSpkMode) {
    spkVolume = spkVolume + 2;
    if(spkVolume > 63) {
      spkVolume = 63;
    }
  } else {
    hpVolume[0] = hpVolume[0] + 2;
    hpVolume[1] = hpVolume[1] + 2;
    if(hpVolume[0] > 63) {
      hpVolume[0] = 63;
    }
    if(hpVolume[1] > 63) {
      hpVolume[1] = 63;
    }
  }
  displayVolume();
}

void displayVolume() {
  M5.Lcd.fillRect(0, 215, 320, 240, M5.Lcd.color565(50,50,50));
  if(isSpkMode) {
    dac.setSPKvol(spkVolume); 
    dac.setHPvol(0, 0);

    M5.Lcd.setTextColor(TFT_ORANGE, M5.Lcd.color565(50,50,50));  
    for(int i=0; i <= spkVolume; i++) {
      M5.Lcd.drawString("|", i*5+5, 220, 2);    
    }
  } else {
    dac.setSPKvol(0); 
    dac.setHPvol(hpVolume[0], hpVolume[1]);

    M5.Lcd.setTextColor(TFT_DARKGREY, M5.Lcd.color565(50,50,50));
    for(int j=0; j <= hpVolume[0]; j++) {
      M5.Lcd.drawString("|", j*5+5, 220, 2);
    }
  }
  
}

void displayHeaders() {
  if(isPlaying) {
    M5.Lcd.fillRect(0, 0, 320, 25, M5.Lcd.color565(50,50,50));
    displayInfo();
    M5.Lcd.fillRect(0, 210, 320, 5, TFT_BLACK);
    M5.Lcd.fillRect(0, 215, 320, 240, M5.Lcd.color565(50,50,50));
    displayVolume();
  } else {
    M5.Lcd.fillRect(0, 0, 320, 25, TFT_BLACK);
    M5.Lcd.fillTriangle(160, 5, 130, 17, 190, 17, TFT_DARKGREY);
    M5.Lcd.fillRect(0, 215, 320, 240, TFT_BLACK);
    M5.Lcd.fillTriangle(130, 222, 160, 234, 190, 222, TFT_DARKGREY);
  }
}

bool readStations(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
      Serial.println("Failed to open file for reading");
      return false;
  }
  int lineIdx = 0;
  String line = "";
  while(file.available()) {
    line = file.readStringUntil('\n');        
    if (line == "") {
      break;
    } 
    if(lineIdx == 0) {
      cfg = line;
    } else {
      // stations
      stationIdx = lineIdx-1;
      stations[stationIdx] = parseLine(line);
    }
    lineIdx++;
  }
  stationsSize = lineIdx;
  file.close();
  return true;
}

Station parseLine(String line) {
  Station st;
  st.name = getValue(line, ',', 0, true);
  st.url = getValue(line, ',', 1, true);
  return st;
}

String getValue(String data, char separator, int index, bool removeQuotes) {
  int found = 0; 
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  String ret = "";
  if(found > index) {
    ret = data.substring(strIndex[0], strIndex[1]);
    ret.trim();
  }
  if(removeQuotes) {
    ret = ret.substring(1, ret.length()-1); //remove quotes    
  }
  return ret;
}


bool readState() {
  Serial.printf("Reading file: %s\n", fileState);

  File file = SD.open(fileState);
  if(!file){
      Serial.println("Failed to open file for reading");
      return false;
  }
  String line = "";
  if(file.available()) {
    line = file.readStringUntil('\n');
    Serial.println(line);
    
    stationIdx  = getValue(line, ',', 0, false).toInt();
    isSpkMode   = getValue(line, ',', 1, false).toInt() > 0 ? true : false;
    spkVolume   = getValue(line, ',', 2, false).toInt();
    hpVolume[0] = getValue(line, ',', 3, false).toInt();
    hpVolume[1] = getValue(line, ',', 4, false).toInt();

    _stationIdx = stationIdx;
    _spkVolume = spkVolume;
    _hpVolume[0] = hpVolume[0];
    _hpVolume[1] = hpVolume[1];

    line = file.readStringUntil('\n');
    Serial.println(line);
    
    red   = getValue(line, ',', 0, false).toInt();
    green = getValue(line, ',', 1, false).toInt();
    blue  = getValue(line, ',', 2, false).toInt();

  }
  file.close();
  return true;
}

void writeState() {
  
  Serial.printf("Writing file: %s\n", fileState);
  
  String line1 = String(stationIdx) +","+ String(isSpkMode) +","+ String(spkVolume) +","+ String(hpVolume[0]) +","+ String(hpVolume[1]);
  Serial.println(line1);
  char line1Buf[line1.length()+1];
  line1.toCharArray(line1Buf, sizeof(line1Buf));

  String line2 = String(red)+","+String(green)+","+String(blue);
  Serial.println(line2);
  char line2Buf[line2.length()+1];
  line2.toCharArray(line2Buf, sizeof(line2Buf));

  File file = SD.open(fileState, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.seek(0);
  file.println(line1Buf);
  file.println(line2Buf);
  file.close();

  _spkVolume = spkVolume;
  _hpVolume[0] = hpVolume[0];
  _hpVolume[1] = hpVolume[1];

}

void readDHT() {
    Serial.print("DHT12, \t");
    int status = DHT.read();
    switch (status)
    {
    case DHT12_OK:
      Serial.print("OK,\t");
      break;
    case DHT12_ERROR_CHECKSUM:
      Serial.print("Checksum error,\t");
      break;
    case DHT12_ERROR_CONNECT:
      Serial.print("Connect error,\t");
      break;
    case DHT12_MISSING_BYTES:
      Serial.print("Missing bytes,\t");
      break;
    default:
      Serial.print("Unknown error,\t");
      break;
    }
    //  DISPLAY DATA, sensor has only one decimal.
    Serial.print(DHT.getHumidity(), 1);
    Serial.print(",\t");
    Serial.println(DHT.getTemperature(), 1);
}

