#ifndef _display_H
#define _display_H

#include "rancilio-pid.h"
const int Display = DISPLAY_HARDWARE;

#include "controls.h"
#include "icon.h"
#if (ICON_COLLECTION == 2)
#include "icon_winter.h"
#elif (ICON_COLLECTION == 1)
#include "icon_smiley.h"
#else
#include "icon_simple.h" // also used as placeholder for ICON_COLLECTION==3
#endif
#include <U8g2lib.h>
#include <Wire.h>

#ifdef ESP32
static int activeStateBuffer;
#endif
static char displaymessagetextBuffer[30];
static char displaymessagetext2Buffer[30];

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#if (DISPLAY == 1)
// Attention: refresh takes around 42ms (esp32: 26ms)!
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE); // e.g. 1.3"
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE); // e.g. 0.96"
#endif
unsigned long previousMillisDisplay = 0; // initialisation at the end of init()
const long intervalDisplay = 1000; // Update für Display
bool image_flip = true;
unsigned int enableScreenSaver = ENABLE_SCREEN_SAVER;
bool screenSaverOn = false;
const int brewReadyWaitPeriod = 300000;
const int userActivityWaitPeriod = 180000;

void u8g2_prepare(void);
bool screenSaverRunning();
void displaymessage(int, char*, char*);
void displaymessage_helper(int, char*, char*);
#endif
void showScreenSaver();
void showMenu(char**, char**);
void showPowerOffCountdown(char*, char*);

extern bool brewReady;
extern unsigned long lastBrewReady;
extern int sleeping;
extern unsigned long userActivity;
extern int activeState;
extern float* activeSetPoint;
extern float steamReadyTemp;
extern float Input;
extern int pidON;
extern int steaming;
extern unsigned long totalBrewTime;
extern const int OnlyPID;
extern unsigned int profile;
extern float* activeBrewtime;
extern float* activePreinfusion;
extern float* activePreinfusionPause;
extern unsigned long brewTimer;
extern bool forceOffline;
extern bool isWifiWorking();
extern bool isBlynkWorking();
extern bool isMqttWorking();
extern int brewing;
extern unsigned long lastBrewEnd;
extern unsigned int powerOffTimer;
extern unsigned int menuPosition;
extern unsigned long previousTimerMenuCheck;
extern const unsigned int menuOffTimer;
extern menuMap* menuConfig;
extern float menuValue;
extern const char* convertDefineToReadAbleVariable(char*);

