/***********************************
 *  DISPLAY
 ***********************************/
#include "display.h"
#include <float.h>

unsigned long previousMillisDisplay = 0; // initialisation at the end of init()
const long intervalDisplay = 1000; // update for display
bool image_flip = true;
unsigned int enableScreenSaver = ENABLE_SCREEN_SAVER;
bool screenSaverOn = false;

// Attention: refresh takes around 42ms (esp32: 26ms)!
#if (DISPLAY_HARDWARE == 1)
  U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, DISPLAY_I2C_SCL, DISPLAY_I2C_SDA); // e.g. 1.3"
#elif (DISPLAY_HARDWARE == 2)
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, DISPLAY_I2C_SCL, DISPLAY_I2C_SDA); // e.g. 0.96"
#else
// 23-MOSI 18-CLK
#define OLED_CS             5
#define OLED_DC             2
U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, /* reset=*/U8X8_PIN_NONE); // e.g. 1.3"
#endif


void u8g2_init(void) {
#ifdef ESP32
#if (DISPLAY_HARDWARE == 3)
  u8g2.setBusClock(600000);
#else
  u8g2.setBusClock(2000000);
#endif
#endif
  u8g2.begin();
  u8g2_prepare();
  u8g2.setFlipMode(ROTATE_DISPLAY);
  u8g2.clearBuffer();
}

void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_profont11_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.setPowerSave(0);
}

bool softwareUpdateCheck() {
  return activeState == STATE_SOFTWARE_UPDATE;
}

bool screenSaverCheck() {
  if ((enableScreenSaver && brewReady && (millis() >= lastBrewReady + brewReadyWaitPeriod) && (millis() >= userActivity + userActivityWaitPeriod)) || sleeping) {
    menuPosition = 0;
    return true;
  } else {
    if (screenSaverOn) {
      u8g2.setPowerSave(0);
      screenSaverOn = false;
    }
    return false;
  }
}

bool menuCheck() {
  if (menuPosition != 0 && (millis() <= previousTimerMenuCheck + menuOffTimer) ) {
    return true;
  } else {
    menuPosition = 0;
    return false;
  }
}

char* outputSimpleState() {
  switch (activeState) {
    case STATE_STEAM_MODE: {
      return (char*)"Steaming";
    }
    case STATE_CLEAN_MODE: {
      return (char*)"Cleaning";
    }
    case STATE_SLEEP_MODE: {
      return (char*)"Sleeping";
    }
  }
  if (!pidON) { return (char*)"Turned off"; }
  if (brewReady) { return (char*)"Ready"; }
  return (char*)""; //"Please wait";
}

void setDisplayTextState(int activeState, char* displaymessagetext, char* displaymessagetext2) {
#if (DISPLAY_TEXT_STATE == 1)
  if (menuPosition != 0) return;
  if (strlen(displaymessagetext) > 0 || strlen(displaymessagetext2) > 0 || screenSaverOn || activeState == STATE_BREW_DETECTED) { // dont show state in certain situations
    snprintf((char*)displaymessagetextBuffer, sizeof(displaymessagetextBuffer), "%s", displaymessagetext);
    snprintf((char*)displaymessagetext2Buffer, sizeof(displaymessagetext2Buffer), "%s", displaymessagetext2);
  } else {
    snprintf((char*)displaymessagetextBuffer, sizeof(displaymessagetextBuffer), "%s", displaymessagetext);
    snprintf((char*)displaymessagetext2Buffer, sizeof(displaymessagetext2Buffer), "%s", outputSimpleState());
  }
#else
  snprintf((char*)displaymessagetextBuffer, sizeof(displaymessagetextBuffer), "%s", displaymessagetext);
  snprintf((char*)displaymessagetext2Buffer, sizeof(displaymessagetext2Buffer), "%s", displaymessagetext2);
#endif
}

#ifdef ESP32
void displaymessage_esp32_task(void* activeStateParam) {
  u8g2_init();
  delay(100);
  for (;;) {
    // unsigned long cur_micros_display = micros();
    displaymessage_helper(activeStateBuffer, displaymessagetextBuffer, displaymessagetext2Buffer);
    // DEBUG_print("inside displaymessage_esp32_task() done =%lu\n", micros()-cur_micros_display);
    vTaskDelay(intervalDisplay / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}
#endif

void displaymessage(int activeState, char* displaymessagetext, char* displaymessagetext2) {
  if (Display > 0) {
    static int only_once = 0;
#ifdef ESP32
    // DEBUG_print("activeState=%d | %s | %s\n", activeState, displaymessagetext, displaymessagetext2);
    if ((millis() >= previousMillisDisplay + intervalDisplay) || only_once == 0) {
      previousMillisDisplay = millis();
      activeStateBuffer = activeState;
      setDisplayTextState(activeStateBuffer, displaymessagetext, displaymessagetext2);
    }
    if (only_once == 0) {
      only_once = 1;
      xTaskCreatePinnedToCore(displaymessage_esp32_task, /* Task function. */
          "displaymessage", /* name of task. */
          2000, /* Stack size of task */
          (void*)&activeState, /* parameter of the task */
          6, /* priority of the task */
          NULL, /* Task handle to keep track of created task */
          0); /* pin task to core 1 */
    }
#else
    if (only_once == 0) {
      only_once = 1;
      u8g2_init();
    }
    if ((millis() >= previousMillisDisplay + intervalDisplay) || previousMillisDisplay == 0) {
      previousMillisDisplay = millis();
      setDisplayTextState(activeState, displaymessagetext, displaymessagetext2);
      displaymessage_helper(activeState, displaymessagetextBuffer, displaymessagetext2Buffer);
    }
#endif
  }
}

void displaymessage_helper(int activeState, char* displaymessagetext, char* displaymessagetext2) {
  u8g2.clearBuffer();
  u8g2.setBitmapMode(1);

  if (softwareUpdateCheck()) {
    showSoftwareUpdate();
  } else if (screenSaverCheck()) {
    showScreenSaver();
  } else if (menuCheck()) {
    showMenu(&displaymessagetext, &displaymessagetext2);
  } else {
    image_flip = !image_flip;
    unsigned int align_right;
    const unsigned int align_right_2digits = LCDWidth - 56;
    const unsigned int align_right_3digits = LCDWidth - 56 - 12;

    bool showLastBrewStatistics = ( (brewTimer > 0) && (currentWeight != 0) && 
     (millis() <= brewStatisticsTimer + brewStatisticsAdditionalDisplayTime) ) ? true : false;

    // boot logo
    if (activeState == STATE_UNDEFINED) {
      if (strcmp(MACHINE_TYPE, "rancilio") == 0) {
        u8g2.drawXBMP(41, 0, rancilio_logo_width, rancilio_logo_height, rancilio_logo_bits);
      } else if (strcmp(MACHINE_TYPE, "gaggia") == 0) {
        u8g2.drawXBMP(5, 0, gaggia_logo_width, gaggia_logo_height, gaggia_logo_bits);
      } else if (strcmp(MACHINE_TYPE, "ecm") == 0) {
        u8g2.drawXBMP(11, 0, ecm_logo_width, ecm_logo_height, ecm_logo_bits);
      } else {
        u8g2.drawXBMP(41, 0, general_logo_width, general_logo_height, general_logo_bits);
      }
    } else {
#if (ICON_COLLECTION == 3)
      // text only mode
      if (strcmp(MACHINE_TYPE, "rancilio") == 0) {
        u8g2.drawXBMP(0, 0, rancilio_logo_width, rancilio_logo_height, rancilio_logo_bits);
      } else {
        u8g2.drawXBMP(0, 0, general_logo_width, general_logo_height, general_logo_bits);
      }
#else
      // display icons
      switch (activeState) {
        case STATE_COLDSTART:
        case STATE_STABILIZE_TEMPERATURE:
          if (image_flip) {
            u8g2.drawXBMP(0, 0, icon_width, icon_height, coldstart_rotate_bits);
          } else {
            u8g2.drawXBMP(0, 0, icon_width, icon_height, coldstart_bits);
          }
          break;
        case STATE_BREW_DETECTED: // brew
          if (image_flip) {
            u8g2.drawXBMP(0, 0, icon_width, icon_height, brewing_bits);
          } else {
            u8g2.drawXBMP(0, 0, icon_width, icon_height, brewing_rotate_bits);
          }
          break;
        case STATE_INNER_ZONE_DETECTED:
          if (brewReady) {
            if (image_flip) {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, brew_ready_bits);
            } else {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, brew_ready_rotate_bits);
            }
          } else { // inner zone
            if (image_flip) {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, brew_acceptable_bits);
            } else {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, brew_acceptable_rotate_bits);
            }
          }
          break;
        case STATE_OUTER_ZONE_DETECTED:
          if (Input >= steamReadyTemp) { // fallback: if hardware steaming button is used still show steaming icon
            if (image_flip) {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, steam_bits);
            } else {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, steam_rotate_bits);
            }
          } else {
            if (image_flip) {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, outer_zone_bits);
            } else {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, outer_zone_rotate_bits);
            }
          }
          break;
        case STATE_STEAM_MODE: // steaming state (detected via controlAction STEAMING)
          if (Input >= steamReadyTemp) {
            if (image_flip) {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, steam_bits);
            } else {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, steam_rotate_bits);
            }
          } else {
            // TODO create new icons for steam phase
            if (image_flip) {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, outer_zone_bits);
            } else {
              u8g2.drawXBMP(0, 0, icon_width, icon_height, outer_zone_rotate_bits);
            }
          }
          break;
        case STATE_SLEEP_MODE: // sleeping state
          break;
        case STATE_CLEAN_MODE: // cleaning state
          if (image_flip) {
            u8g2.drawXBMP(0, 0, icon_width, icon_height, clean_bits);
          } else {
            u8g2.drawXBMP(0, 0, icon_width, icon_height, clean_rotate_bits);
          }
          break;
      }
#endif
    }

    // display current and target temperature
    if (activeState > STATE_UNDEFINED && activeState != STATE_BREW_DETECTED && !showLastBrewStatistics) {
      if (Input - 100 > -FLT_EPSILON) {
        align_right = align_right_3digits;
      } else {
        align_right = align_right_2digits;
      }
      u8g2.setFont(u8g2_font_profont22_tf);
      u8g2.setCursor(align_right, 3);
      u8g2.print(Input, 1);
      u8g2.setFont(u8g2_font_profont10_tf);
      u8g2.print((char)176);
      u8g2.println("C");
      u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
      u8g2.drawGlyph(align_right - 11, 3 + 6, 0x0046);

      // if (Input <= *activeSetPoint + 5 || activeState == STATE_STEAM_MODE) { //only show setpoint if we are not steaming
      if (!steaming) {
        if (*activeSetPoint >= 100) {
          align_right = align_right_3digits;
        } else {
          align_right = align_right_2digits;
        }
        u8g2.setFont(u8g2_font_profont22_tf);
        u8g2.setCursor(align_right, 20);
        u8g2.print(*activeSetPoint, 1);
        u8g2.setFont(u8g2_font_profont10_tf);
        u8g2.print((char)176);
        u8g2.println("C");
        u8g2.setFont(u8g2_font_open_iconic_other_1x_t);
        u8g2.drawGlyph(align_right - 11, 20 + 6, 0x047);
      }
    } else if (activeState == STATE_BREW_DETECTED || showLastBrewStatistics) {  //brew
      totalBrewTime = ( (OnlyPID || BREWTIME_TIMER == 0 )? *activeBrewTime : *activePreinfusion + *activePreinfusionPause + *activeBrewTime) * 1000;
      unsigned int align_right_left_value = LCDWidth - 56 - 5;
      unsigned int align_right_right_value = LCDWidth - 56 + 28;
      u8g2.setFont(u8g2_font_profont22_tf);
      u8g2.setCursor(align_right_left_value, 3);
      if (brewTimer < 10000) u8g2.print("0");
      // TODO: Use print(u8x8_u8toa(value, digits)) or print(u8x8_u16toa(value, digits)) to print numbers with constant width (numbers are prefixed with 0 if required).
      u8g2.print(brewTimer / 1000);

      u8g2.setFont(u8g2_font_open_iconic_arrow_1x_t);
      u8g2.drawGlyph(align_right_right_value - 8, 3 + 6, 0x04e);
      u8g2.setFont(u8g2_font_profont22_tf);
      u8g2.setCursor(align_right_right_value, 3);
      u8g2.print(totalBrewTime / 1000);

      u8g2.setFont(u8g2_font_profont10_tf);
      u8g2.println("s");

      if (SCALE_SENSOR_ENABLE) {
        u8g2.setFont(u8g2_font_profont22_tf);
        u8g2.setCursor(align_right_left_value, 20);
        int weight = (int) currentWeight;
        //if (weight <0) weight = 0;
        if (weight < 10) u8g2.print("0");
        u8g2.print(weight<0?0:weight, 0);

        u8g2.setFont(u8g2_font_open_iconic_arrow_1x_t);
        u8g2.drawGlyph(align_right_right_value - 8, 20 + 6, 0x04e);
        u8g2.setFont(u8g2_font_profont22_tf);
        u8g2.setCursor(align_right_right_value, 20);
        u8g2.print(*activeScaleSensorWeightSetPoint, 0);

        u8g2.setFont(u8g2_font_profont10_tf);
        u8g2.println("g");
      }
      //u8g2.setFont(u8g2_font_open_iconic_other_1x_t);
      u8g2.setFont(u8g2_font_open_iconic_thing_1x_t);
      if (*activeBrewTimeEndDetection == 0) {
        u8g2.drawGlyph(align_right_left_value - 11, 3 + 6, 0x04f);
      } else {
        u8g2.drawGlyph(align_right_left_value - 11, 20 + 6, 0x04f);
      }
    }
  }

  // power-off timer
#if (ENABLE_POWER_OFF_COUNTDOWN > 0)
  showPowerOffCountdown(displaymessagetext, displaymessagetext2);
#endif

  //(optional) add 2 text lines
  u8g2.setFont(u8g2_font_profont11_tf);
  u8g2.setCursor(ALIGN_CENTER(displaymessagetext), 44); // 9 pixel space between lines
  u8g2.print(displaymessagetext);
  u8g2.setCursor(ALIGN_CENTER(displaymessagetext2), 53);
  u8g2.print(displaymessagetext2);

  // add status icons
  if (millis() >= 10000) {
    byte icon_y = 64 - (status_icon_height - 1);
    byte icon_counter = 0;
    #if (ENABLE_PROFILE_STATUS > 0)
      if (profile == 1 && ENABLE_PROFILE_STATUS == 1 && !screenSaverOn) { u8g2.drawXBMP(icon_counter * (status_icon_width - 1), icon_y, status_icon_width, status_icon_height, profile_1_bits); icon_counter++; }
      else if (profile == 2 && !screenSaverOn) { u8g2.drawXBMP(icon_counter * (status_icon_width - 1), icon_y, status_icon_width, status_icon_height, profile_2_bits); icon_counter++; }
      else if (profile == 3 && !screenSaverOn) { u8g2.drawXBMP(icon_counter * (status_icon_width - 1), icon_y, status_icon_width, status_icon_height, profile_3_bits); icon_counter++; }
    #endif
    #if (ENABLE_FAILURE_STATUS_ICONS == 1)
      if (image_flip) {   
        if ((!forceOffline && !isWifiWorking()) || (forceOffline && !FORCE_OFFLINE)) {
          u8g2.drawXBMP(icon_counter * (status_icon_width - 1), icon_y, status_icon_width, status_icon_height, wifi_not_ok_bits);
          icon_counter++;
        }
        if (BLYNK_ENABLE && !isBlynkWorking() && !FORCE_OFFLINE) {
          u8g2.drawXBMP(icon_counter * (status_icon_width - 1), icon_y, status_icon_width, status_icon_height, blynk_not_ok_bits);
          icon_counter++;
        }
        if (MQTT_ENABLE && !isMqttWorking() && !FORCE_OFFLINE) {
          u8g2.drawXBMP(icon_counter * (status_icon_width - 1), icon_y, status_icon_width, status_icon_height, mqtt_not_ok_bits);
          icon_counter++;
        }
    }
    #endif
  }
  u8g2.sendBuffer();
}

void showSoftwareUpdate() {
    u8g2.drawXBMP(41, 0, icon_width, general_logo_height, update_bits);
}

void showScreenSaver() {
  static unsigned int screen_saver_x_pos = 41;
  static bool screen_saver_direction_right = true;
  const int unsigned screen_saver_step = 4;
  unsigned int logo_width = icon_width;
  if (enableScreenSaver == 3 && strcmp(MACHINE_TYPE, "gaggia") == 0) {
    logo_width = 125; // hack which will result in logo only moving left
    screen_saver_x_pos = 5;
  } else if (enableScreenSaver == 3 && strcmp(MACHINE_TYPE, "ecm") == 0) {
    logo_width = 125; // hack which will result in logo only moving left
    screen_saver_x_pos = 11;
  }
  if (screen_saver_direction_right) {
    if (screen_saver_x_pos + screen_saver_step <= LCDWidth - logo_width) {
      screen_saver_x_pos += screen_saver_step;
    } else {
      screen_saver_x_pos -= screen_saver_step;
      screen_saver_direction_right = false;
    }
  } else {
    if (screen_saver_x_pos >= screen_saver_step) {
      screen_saver_x_pos -= screen_saver_step;
    } else {
      screen_saver_x_pos += screen_saver_step;
      screen_saver_direction_right = true;
    }
  }
  if (enableScreenSaver == 1 || sleeping) {
    if (!screenSaverOn) { u8g2.setPowerSave(1); }
  } else if (enableScreenSaver == 2) {
    u8g2.drawXBMP(screen_saver_x_pos, 0, icon_width, icon_height, brew_ready_bits);
  } else if (enableScreenSaver == 3) {
    if (strcmp(MACHINE_TYPE, "rancilio") == 0) {
      u8g2.drawXBMP(screen_saver_x_pos, 0, rancilio_logo_width, rancilio_logo_height, rancilio_logo_bits);
    } else if (strcmp(MACHINE_TYPE, "gaggia") == 0) {
      u8g2.drawXBMP(screen_saver_x_pos, 0, gaggia_logo_width, gaggia_logo_height, gaggia_logo_bits); // TODO fix
    } else if (strcmp(MACHINE_TYPE, "ecm") == 0) {
      u8g2.drawXBMP(screen_saver_x_pos, 0, ecm_logo_width, ecm_logo_height, ecm_logo_bits); // TODO fix
    }
  }
  screenSaverOn = true;
}

void showMenu(char** displaymessagetext, char** displaymessagetext2) {
  image_flip = !image_flip;
  unsigned int align_right;
  const unsigned int align_right_2digits = LCDWidth - 56;
  const unsigned int align_right_3digits = LCDWidth - 56 - 12;
  const unsigned int align_right_1digits_decimal = LCDWidth - 56 + 12;
  menuMap* menuConfigPosition = getMenuConfigPosition(menuConfig, menuPosition);
  if (!menuConfigPosition) return;
  if (image_flip) {
    u8g2.drawXBMP(0, 0, icon_width, icon_height, menu_rotate_bits);
  } else {
    u8g2.drawXBMP(0, 0, icon_width, icon_height, menu_bits);
  }
  u8g2.setFont(u8g2_font_profont22_tf);
  if (!strcmp(menuConfigPosition->value->type, "bool")) {
    bool menuValue;
    if (menuConfigPosition->value->is_double_ptr) {
      menuValue = **(int**)menuConfigPosition->value->ptr;
    } else {
      menuValue = *(int*)menuConfigPosition->value->ptr;
    }
    u8g2.setCursor(align_right_2digits, 3);
    if ( menuValue == 0) {
      u8g2.print("Off");
    } else {
      u8g2.print("On");
    }
  }
  else if (!strcmp(menuConfigPosition->value->type, "int")) {
    int menuValue;
    if (menuConfigPosition->value->is_double_ptr) {
      menuValue = **(int**)menuConfigPosition->value->ptr;
    } else {
      menuValue = *(int*)menuConfigPosition->value->ptr;
    }
    if (menuValue >= 100) {
          align_right = align_right_3digits;
        } else {
          if (menuValue >= 10) {
            align_right = align_right_2digits;
          } else align_right = align_right_1digits_decimal;
    }
    u8g2.setCursor(align_right, 3);
    u8g2.print(menuValue, 1);
  } else {
    float menuValue;
    if (menuConfigPosition->value->is_double_ptr) {
      menuValue = **(float**)menuConfigPosition->value->ptr;
    } else {
      menuValue = *(float*)menuConfigPosition->value->ptr;
    }
    if (menuValue - 100 > -FLT_EPSILON) {
          align_right = align_right_3digits;
        } else {
          if (menuValue >= 10) {
            align_right = align_right_2digits;
          } else align_right = align_right_1digits_decimal;
    }
    u8g2.setCursor(align_right, 3);
    u8g2.print(menuValue, 1);
  }
  char* unit = menuConfigPosition->unit;
  if (!unit) {
  } else if (strcmp(unit, "C") == 0) {
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.print((char)176);
    u8g2.println(unit);
  } else {
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.println(unit);
  }
  *displaymessagetext = (char*)"";
  *displaymessagetext2 = (char*) convertDefineToReadAbleVariable(menuConfigPosition->item);
}

/*
char* camelCase(char line[])  {
    static char buffer[30];
    snprintf(buffer, sizeof(buffer), "%s", line);
    bool active = true;

    for(int i = 0; buffer[i] != '\0'; i++) {
        if(std::isalpha(buffer[i])) {
            if(active) {
                buffer[i] = toupper(buffer[i]);
                active = false;
            } else {
                buffer[i] = tolower(buffer[i]);
            }
        } else if(buffer[i] == '_') {
            active = true;
            buffer[i] = ' ';
        } else if(buffer[i] == ' ') {
            active = true;
        }
    }
    return buffer;
}
*/

  const unsigned int powerOffCountDownStart = 300;
void showPowerOffCountdown(char* displaymessagetext, char* displaymessagetext2) {
  const unsigned int align_right_countdown_min = LCDWidth - 52;
  const unsigned int align_right_countdown_sec = LCDWidth - 52 + 20;
  static char line[30];
  powerOffTimer = ENABLE_POWER_OFF_COUNTDOWN - ((millis() - lastBrewEnd) / 1000);
  if (powerOffTimer <= powerOffCountDownStart && !brewing && !strlen(displaymessagetext) && !strlen(displaymessagetext2)) {
    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    u8g2.drawGlyph(align_right_countdown_min - 15, 37 + 6, 0x004e);
    u8g2.setFont(u8g2_font_profont22_tf);
    u8g2.setCursor(align_right_countdown_min, 37);
    snprintf(line, sizeof(line), "%d", int(powerOffTimer / 60));
    u8g2.print(line);
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.println("m");
    u8g2.setFont(u8g2_font_profont22_tf);
    u8g2.setCursor(align_right_countdown_sec, 37);
    snprintf(line, sizeof(line), "%02d", int(powerOffTimer % 60));
    u8g2.print(line);
    u8g2.setCursor(align_right_countdown_sec + 23, 37);
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.println(" s");
  }
}