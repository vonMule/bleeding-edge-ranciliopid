#ifndef _display_H
#define _display_H

#include "icon.h"
#if (ICON_COLLECTION == 1)
#include "icon_smiley.h"
#else
#include "icon_simple.h"
#endif
#include <U8g2lib.h>
#include <Wire.h>
//#define OLED_RESET 16   // Output pin for disply reset pin
#define OLED_SCL 5        // Output pin for dispaly clock pin
#define OLED_SDA 4        // Output pin for dispaly data pin
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

void u8g2_prepare(void);
bool screenSaverRunning();
void displaymessage(int, char*, char*);

#endif
