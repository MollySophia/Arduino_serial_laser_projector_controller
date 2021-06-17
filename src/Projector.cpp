#include <Arduino.h>
#include <ESP8266WiFi.h>

// OTA Includes
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include <EEPROM.h>

const char *ssid         = "Molly_2.4G";
const char *password     = "qazwsx741";
int pinA = 14;
int pinB = 12;
int pinSW = 13;

#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "images.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3c
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int menuID = 0, listID = 0, listStr = 0;
int keyState = 0;
int curCLK, lastCLK;
int rotatorDiff = 0, rotatorDiffLast = 0;
char powerState = 0, keystoneVertical = 0, keystoneHorizontal = 0, brightnessVal = 0, contrastVal = 0, flipVal = 0;
unsigned long ms = 0;


IRAM_ATTR void keyISR() {
  ms = millis();
  while(digitalRead(pinSW) == LOW);
  if(millis() - ms >= 300) keyState = 2;
  else keyState = 1;
}

IRAM_ATTR void rotatorISR() {
  curCLK = digitalRead(pinA);

  if(curCLK != lastCLK && curCLK == 0) {
    if(digitalRead(pinB) != curCLK) rotatorDiff++;
    else rotatorDiff--;
  }

  lastCLK = curCLK;
  //delay(1);
}

void powerOnOff();
void flipScreen(char value);
void brightness(char value);
void contrast(char value);
void keystoneCorrectionHorizontal(char value);
void keystoneCorrectionVertical(char value);
void enterFactoryMode();
void factoryModeExit(bool save);
void saveUserConfig();
void restoreDefaultConfig();

void drawCenterString(const String &buf, int x, int y);
void drawProgressBar(int x, int y, int w, int h, int percentage);
void drawTitle(const char *string);
void drawListEntry(String string, int num, int selected);

#define MENU_MAX_NUM 5
#define FLIP_MAX_NUM 3
String menuList[6] = {
  "Flip",
  "Keystone",
  "Brightness",
  "Contrast",
  "Save",
  "Restore"
};

String flipList[4] = {
  "Flip both",
  "Y-Flip",
  "X-Flip",
  "Normal"
};

void setup() {
  WiFi.begin (ssid, password);

  // Wait for connection
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(10);
  // }

  EEPROM.begin(5);
  keystoneVertical = EEPROM.read(0);
  keystoneHorizontal = EEPROM.read(1);
  brightnessVal = EEPROM.read(2);
  contrastVal = EEPROM.read(3); 
  flipVal = EEPROM.read(4);

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.drawBitmap(0, 0, bootsplash, 128, 64, SSD1306_WHITE);
  display.display();

  display.setTextSize(2);      // Normal 1:1 pixel scale
  //display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setTextColor(SSD1306_INVERSE);
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    display.clearDisplay();
    drawCenterString("OTA Update", 64, 32 - 20);
    display.display();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    drawProgressBar(4, 32, 120, 8, progress / (total / 100));
    display.display();
  });

  ArduinoOTA.onEnd([]() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("Update completed.\nRebooting...\n");
    display.display();
  });

  Serial.begin(9600);
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(pinSW), keyISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(pinA), rotatorISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pinB), rotatorISR, CHANGE);
  lastCLK = digitalRead(pinA);


  delay(3000);
}

void actionHandle() {
  while(keyState == 0 && (rotatorDiff - rotatorDiffLast == 0)) {
    ArduinoOTA.handle();
    delay(100);
  }

  int tmp_val = 0;
  char tmp_string[10];

  switch (menuID) {
    case 0: //power
      if(keyState == 1) {
        powerOnOff();
        display.clearDisplay();
        if(powerState == 0) {
          drawCenterString("Starting", 64, 32 - 20);
          powerState = 1;
        } else {
          drawCenterString("Stopping", 64, 32 - 20);
          powerState = 0;
        }
        keyState = 0;
        for(int i = 0; i < 10; i++) {
          drawProgressBar(4, 32, 120, 8, i * 10 );
          display.display();
          delay(400);
        }
        flipScreen(flipVal);
        delay(50);
        brightness(brightnessVal);
        delay(50);
        keystoneCorrectionVertical(keystoneVertical);
        delay(50);
        keystoneCorrectionHorizontal(keystoneHorizontal);
        delay(50);
        contrast(contrastVal);
      } else if(keyState == 2) {
        menuID = 1;
        keyState = 0;
        listID = 0;
        listStr = 0;
      }
      break;

    case 1: //menu
      if(keyState == 1) {
        switch (listID) {
          case 0: 
            menuID = 2; //flip
            break;
          case 1:
            menuID = 3; //keystone
            break;
          case 2:
            menuID = 4; //brightness
            break;
          case 3:
            menuID = 5; //contrast
            break;
          case 4:
            saveUserConfig();
            delay(500);
            break;
          case 5:
            restoreDefaultConfig();
            delay(500);
            break;
        }
        listStr = 0;
        listID = 0;
      } else if(keyState == 2) {
        menuID = 0;
        keyState = 0;
      }
      if(rotatorDiff != rotatorDiffLast) {
        if(rotatorDiff > rotatorDiffLast)
          listID++;
        else listID--;

        if(listID < 0) listID = 0;
        if(listID > MENU_MAX_NUM) listID = MENU_MAX_NUM;
        if(listID - listStr >= 3) listStr++;
        if(listID - listStr < 0) listStr = listID;
      }
      break;

    case 2://flip settings
      if(keyState == 1) {
        flipScreen(listID);
        flipVal = listID;
        delay(500);
        listStr = 0;
        listID = 0;
        menuID = 1;
      }
      if(rotatorDiff != rotatorDiffLast) {
        if(rotatorDiff > rotatorDiffLast)
          listID++;
        else listID--;

        if(listID < 0) listID = 0;
        if(listID > FLIP_MAX_NUM) listID = FLIP_MAX_NUM;
        if(listID - listStr >= 3) listStr++;
        if(listID - listStr < 0) listStr = listID;
      }
      break;

    case 3://keystone
      if(keyState == 1) {
        keyState = 0;
        display.clearDisplay();
        if(listID == 0)  
          tmp_val = keystoneVertical;
        else tmp_val = keystoneHorizontal;
        while(keyState == 0) {
          if(rotatorDiff - rotatorDiffLast > 0) tmp_val++;
          if(rotatorDiff - rotatorDiffLast < 0) tmp_val--;
          rotatorDiffLast = rotatorDiff;
          if(tmp_val > 30) tmp_val = 30;
          if(listID == 0 && tmp_val < -15) tmp_val = -15;
          if(listID == 0 && tmp_val < -30) tmp_val = -30;
          sprintf(tmp_string, "- %i +", tmp_val);
          display.clearDisplay();
          drawCenterString(tmp_string, 64, 32);
          display.display();
          if(listID == 0)  {
            keystoneCorrectionVertical(tmp_val);
            keystoneVertical = tmp_val;
          }
          else {
            keystoneCorrectionHorizontal(tmp_val);
            keystoneHorizontal = tmp_val;
          }
          delay(200);
        }
        listID = 0;
        listStr = 0;
        menuID = 1;
      }
      if(rotatorDiff != rotatorDiffLast) {
        if(rotatorDiff > rotatorDiffLast)
          listID++;
        else listID--;

        if(listID < 0) listID = 0;
        if(listID > 1) listID = 1;
        if(listID - listStr >= 3) listStr++;
        if(listID - listStr < 0) listStr = listID;
      }
      break;
    
    case 4: //brightness
      display.clearDisplay();
      tmp_val = brightnessVal;
      keyState = 0;
      while(keyState == 0) {
        if(rotatorDiff - rotatorDiffLast > 0) tmp_val++;
        if(rotatorDiff - rotatorDiffLast < 0) tmp_val--;
        rotatorDiffLast = rotatorDiff;
        if(tmp_val > 10) tmp_val = 10;
        if(tmp_val < -31) tmp_val = -31;
        sprintf(tmp_string, "- %i +", tmp_val);
        display.clearDisplay();
        drawCenterString(tmp_string, 64, 32);
        display.display();
        brightnessVal = tmp_val;
        brightness(brightnessVal);
        delay(200);
      }
      listID = 0;
      listStr = 0;
      menuID = 1;
      break;

    case 5: //contrast
      display.clearDisplay();
      tmp_val = contrastVal;
      keyState = 0;
      while(keyState == 0) {
        if(rotatorDiff - rotatorDiffLast > 0) tmp_val++;
        if(rotatorDiff - rotatorDiffLast < 0) tmp_val--;
        rotatorDiffLast = rotatorDiff;
        if(tmp_val > 15) tmp_val = 15;
        if(tmp_val < -15) tmp_val = -15;
        sprintf(tmp_string, "- %i +", tmp_val);
        display.clearDisplay();
        drawCenterString(tmp_string, 64, 32);
        display.display();
        contrastVal = tmp_val;
        contrast(contrastVal);
        delay(200);
      }
      listID = 0;
      listStr = 0;
      menuID = 1;
      break;

    default:
      break;
  }

  rotatorDiffLast = rotatorDiff;
  keyState = 0;
}

void uiHandle() {
  switch(menuID) {
    case 0:
      display.clearDisplay();
      if(powerState == 0)
        display.drawBitmap(0, 0, poweroff, 128, 64, SSD1306_WHITE);
      else
        display.drawBitmap(0, 0, poweron, 128, 64, SSD1306_WHITE);
      display.display();
      break;

    case 1: //menu
      display.clearDisplay();
      drawTitle("Menu");

      for(int i = 0; i < 3; i ++) {
        drawListEntry(menuList[listStr + i], i+1, listStr+i == listID ? 1: 0);
      }

      display.display();
      break;
    
    case 2: //flip
      display.clearDisplay();
      drawTitle("Flip");

      for(int i = 0; i < 3; i ++) {
        drawListEntry(flipList[listStr + i], i+1, listStr+i == listID ? 1: 0);
      }

      display.display();
      break;

    case 3: //keystone
      display.clearDisplay();
      drawTitle("Keystone");
      drawListEntry("Vertical", 1, listID == 0 ? 1 : 0);
      drawListEntry("Horizontal", 2, listID == 1 ? 1 : 0);
      display.display();
      break;

    default: 
      break;  
  }
}

void loop() {
  uiHandle();
  actionHandle();
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
  EEPROM.write(0, keystoneVertical);
  EEPROM.write(1, keystoneHorizontal);
  EEPROM.write(2, brightnessVal);
  EEPROM.write(3, contrastVal); 
  EEPROM.write(4, flipVal);
  EEPROM.commit();
}

void restoreDefaultConfig() {
  unsigned char tmp[8] = {0xff, 0x07, 0x77, 0x00, 0x00, 0x00, 0x00, 0x7e};
  Serial.write(tmp, 8);
  delay(100);
  keystoneHorizontal = 0;
  keystoneVertical = 0;
  brightnessVal = 0;
  contrastVal = 0;
  flipVal = 0;
  EEPROM.write(0, keystoneVertical);
  EEPROM.write(1, keystoneHorizontal);
  EEPROM.write(2, brightnessVal);
  EEPROM.write(3, contrastVal); 
  EEPROM.write(4, flipVal);
  EEPROM.commit();
}

void drawProgressBar(int x, int y, int w, int h, int percentage) {
  display.drawRoundRect(x, y, w, h, 5, SSD1306_WHITE);
  display.fillRoundRect(x + 2, y + 2, (int)((w - 4) / 100.0 * percentage), h - 4, 3, SSD1306_WHITE);
}

void drawCenterString(const String &buf, int x, int y) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    display.setCursor(x - w / 2, y);
    display.print(buf);
}

void drawTitle(const char *string) {
  drawCenterString(string, 64, 0);
  display.drawFastHLine(0, 17, 128, SSD1306_WHITE);
  display.drawFastHLine(0, 16, 128, SSD1306_WHITE);
}

void drawListEntry(String string, int num, int selected) {
  switch(num) {
    case 1:
      if(selected)
        display.fillRect(0, 19, 128, 15, SSD1306_WHITE);
      display.setCursor(2, 19);
      display.print(string);
      break;
    case 2:
      if(selected)
        display.fillRect(0, 34, 128, 15, SSD1306_WHITE);
      display.setCursor(2, 35);
      display.print(string);
      break;
    case 3:
      if(selected)
        display.fillRect(0, 49, 128, 15, SSD1306_WHITE);
      display.setCursor(2, 50);
      display.print(string);
      break;
  }
}