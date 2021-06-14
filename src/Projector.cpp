#include <Arduino.h>
#include <ESP8266WiFi.h>

// OTA Includes
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

const char *ssid         = "Molly_2.4G";
const char *password     = "qazwsx741";
int pinA = 14;
int pinB = 12;
int pinSW = 13;

#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`

SSD1306 display(0x3c, 4, 5);
int menuID = 0;
int keyState = 0;
int rotatorState = 0;

IRAM_ATTR void keyISR() {
  while(digitalRead(pinSW) == LOW);
  keyState = 1;
}

void powerOnOff() {
  unsigned char tmp[8] = {0xff, 0x07, 0x99, 0x00, 0x00, 0x00, 0x00, 0xa0};
  Serial.write(tmp, 8);
  delay(100);
}

void flipScreen(char value) { //0:both 1:vertical 2:horizontal 3:normal
  if(value > 4 || value < 0) return;
  unsigned char tmp[8] = {0xff, 0x07, 0x37, 0x00, 0x00, 0x00, 0x00, 0x3e};
  tmp[3] += value;
  tmp[7] += value;
  Serial.write(tmp, 8);
  delay(100);
}

void brightness(char value) { //-31 ~ 10
  if(value > 10 || value < -31) return;
  unsigned char tmp[8] = {0xff, 0x07, 0x29, 0x00, 0x00, 0x00, 0x00, 0x30};
  tmp[3] += value;
  tmp[7] += value;
  Serial.write(tmp, 8);
  delay(100);
}

void contrast(char value) {
  if(value > 15 || value < -15) return;
  unsigned char tmp[8] = {0xff, 0x07, 0x2b, 0x00, 0x00, 0x00, 0x00, 0x32};
  tmp[3] += value;
  tmp[7] += value;
  Serial.write(tmp, 8);
  delay(100);
}

void keystoneCorrectionHorizontal(char value) {
  if(value > 30 || value < -30) return;
  unsigned char tmp[8] = {0xff, 0x07, 0x33, 0x00, 0x00, 0x00, 0x00, 0x3a};
  tmp[3] += value;
  tmp[7] += value;
  Serial.write(tmp, 8);
  delay(100);
}

void keystoneCorrectionVertical(char value) {
  if(value > 30 || value < -15) return;
  unsigned char tmp[8] = {0xff, 0x07, 0x35, 0x00, 0x00, 0x00, 0x00, 0x3c};
  tmp[3] += value;
  tmp[7] += value;
  Serial.write(tmp, 8);
  delay(100);
}

void enterFactoryMode() {
  unsigned char tmp[8] = {0xff, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, 0x0a};
  Serial.write(tmp, 8);
  delay(100);
}

void factoryModeExit(bool save) {
  unsigned char tmp[8] = {0xff, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x08};
  Serial.write(tmp, 8);
  delay(100);
}

void saveUserConfig() {
  unsigned char tmp[8] = {0xff, 0x07, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x12};
  Serial.write(tmp, 8);
  delay(100);
}

void restoreDefaultConfig() {
  unsigned char tmp[8] = {0xff, 0x07, 0x77, 0x00, 0x00, 0x00, 0x00, 0x7e};
  Serial.write(tmp, 8);
  delay(100);
}

void setup() {
  WiFi.begin (ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
  }

  display.init();
  display.flipScreenVertically();
  display.setContrast(255);

  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 10, "OTA Update");
    display.display();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    display.drawProgressBar(4, 32, 120, 8, progress / (total / 100) );
    display.display();
  });

  ArduinoOTA.onEnd([]() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
    display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, "Restart");
    display.display();
  });

  Serial.begin(9600);
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(pinSW), keyISR, FALLING);
}

void loop() {
  switch(menuID) {
    case 0:
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
      display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, "Press to power on.");
      display.display();
      while(keyState == 0) delay(100);
      display.clear();
      display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2 - 10, "Starting...");
      display.display();
      menuID = 1;
      powerOnOff();
      for(int i = 0; i < 10; i++) {
        display.drawProgressBar(4, 32, 120, 8, i * 10 );
        display.display();
        delay(400);
      }
      keyState = 0;
      break;
    case 1:
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
      display.drawString(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, "Press to power off.");
      display.display();
      while(keyState == 0) delay(100);
      menuID = 0;
      powerOnOff();
      delay(5000);
      keyState = 0;
      break;
    case 100: //ota
      while(1) {
        ArduinoOTA.handle();
      }
      break;
    default: menuID = 1; break;  
  }
}
