#ifndef _eeprom_pid_H
#define _eeprom_pid_H

#include "userConfig.h"

#ifdef ESP32
#include <Preferences.h>
#include <WiFi.h>
Preferences preferences;
#else
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#endif

void sync_eeprom();
void sync_eeprom(bool startup_read, bool force_read);

#define expectedEepromVersion 9 // EEPROM values are saved according to this versions layout. Increase
                                // if a new layout is implemented.

#endif