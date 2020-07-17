
#include <M5Stack.h>

#define RELAY_PIN 2   //GPIO2  solid state relay control
#define PRESS_PIN 5   //GPIO5  pressure switch (yellow)
#define COUNT_PIN 34  //GPIO34 consumed water volume counter 0.5 liter/pulse (blue)
#define FLOOD_PIN 13  //GPIO13 flood sensing (red)

#define RX_PIN 16  //GPIO16 RS485 RX
#define TX_PIN 17  //GPIO17 RS485 TX

#define X_LOCAL 40
#define Y_LOCAL 40

#define X_OFF 160
#define Y_OFF 30

int i=0,s=0;

void header(const char *string, uint16_t color){
    M5.Lcd.fillScreen(color);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_MAGENTA, TFT_BLUE);
    M5.Lcd.fillRect(0, 0, 320, 30, TFT_BLUE);
    M5.Lcd.setTextDatum(TC_DATUM);
    M5.Lcd.drawString(string, 160, 3, 4);
}


void setup() {

  M5.begin();
  M5.Power.begin();

  header("RS485 Master", TFT_BLACK);
  
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  // debug init
  Serial.begin(9600);
  // RS485 init
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);  

  // I/O init
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(PRESS_PIN, INPUT);
  pinMode(COUNT_PIN, INPUT);
  pinMode(FLOOD_PIN, INPUT);
}

void loop() {
  
  if(Serial2.available()){
    char c = Serial2.read();
    i++;
  }
  M5.Lcd.setCursor(X_LOCAL+X_OFF, Y_LOCAL,2);
  M5.Lcd.printf("R: %d\n", i);
  delay(10);
  
}
