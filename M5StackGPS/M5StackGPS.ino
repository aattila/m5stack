/*
  please add TinyGPSPlus to your library first........
  TinyGPSPlus file in M5stack lib examples -> modules -> GPS -> TinyGPSPlus-1.0.2.zip
*/

#include <M5Stack.h>
#include <TinyGPS++.h>

static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
HardwareSerial ss(2);

void setup() {
  M5.begin();
  ss.begin(GPSBaud);
  M5.Lcd.println(F("Latitude   Longitude   Alt    Course Speed Card "));
  M5.Lcd.println(F("(deg)      (deg)       (m)    --- from GPS ---- "));
  M5.Lcd.println(F("------------------------------------------------"));
}

void loop() {
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 30);
  M5.Lcd.setTextColor(WHITE, BLACK);

  printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
  printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
  printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);
  printFloat(gps.course.deg(), gps.course.isValid(), 7, 2);
  printFloat(gps.speed.kmph(), gps.speed.isValid(), 6, 2);
  printStr(gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.deg()) : "*** ", 6);

  M5.Lcd.println();

  smartDelay(1000);

  if (millis() > 5000 && gps.charsProcessed() < 10)
    M5.Lcd.println(F("No GPS data received: check wiring"));

  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 210);
  printDateTime(gps.date, gps.time);

}

static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do 
  {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec) {
  if (!valid) {
    while (len-- > 1)
      M5.Lcd.print('*');
    M5.Lcd.print(' ');
  }
  else {
    M5.Lcd.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      M5.Lcd.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len) {
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  M5.Lcd.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t) {
  if (!d.isValid()) {
    M5.Lcd.print(F("********** "));
  }
  else {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    M5.Lcd.print(sz);
  }
  
  if (!t.isValid()) {
    M5.Lcd.print(F("******** "));
  }
  else {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    M5.Lcd.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}

static void printStr(const char *str, int len) {
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    M5.Lcd.print(i<slen ? str[i] : ' ');
  smartDelay(0);
}
