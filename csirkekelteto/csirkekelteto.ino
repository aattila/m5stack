
#include <M5Stack.h>
#include <Wire.h>
#include "DHT12.h"

DHT12 dht12;               //Preset scale CELSIUS and ID 0x5c.
#define DHT_ADDR    0x5c   //Env module's DHT sensor address

#define NOTE_D0 -1
#define NOTE_D1 294
#define NOTE_D2 330
#define NOTE_D3 350
#define NOTE_D4 393
#define NOTE_D5 441
#define NOTE_D6 495
#define NOTE_D7 556

#define NOTE_DL1 147
#define NOTE_DL2 165
#define NOTE_DL3 175
#define NOTE_DL4 196
#define NOTE_DL5 221
#define NOTE_DL6 248
#define NOTE_DL7 278

#define NOTE_DH1 589
#define NOTE_DH2 661
#define NOTE_DH3 700
#define NOTE_DH4 786
#define NOTE_DH5 882
#define NOTE_DH6 990
#define NOTE_DH7 112


uint8_t music[1000];

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

void setup() {

  M5.begin();
  M5.Power.begin();
  Wire.begin();
  
}

void loop() {
  
  float tmp = dht12.readTemperature();
  float hum = dht12.readHumidity();
  String temp = String(tmp, 1);
  String humi = String(hum, 1);

  int tempColor = TFT_GREEN;
  int humiColor = TFT_BLUE;
  int alertColor = TFT_RED;

  if(tmp < 38 or tmp > 38.8) {
    tempColor = alertColor;
    beep(200);
    delay(500);
  }
  if(hum < 50.5) {
    humiColor = alertColor;
    beep(200);
    delay(500);
  }
  
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setTextColor(tempColor, TFT_BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString(temp, 20, 10, 7);
  M5.Lcd.setTextSize(3);
  M5.Lcd.drawString("C", 250, 10, 4);

  M5.Lcd.setTextColor(humiColor, TFT_BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString(humi, 20, 130, 7);
  M5.Lcd.setTextSize(3);
  M5.Lcd.drawString("%", 250, 130, 4);

  delay(2000);
//  M5.update();
}
