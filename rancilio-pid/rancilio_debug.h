#ifndef _rancilio_debug_H
#define _rancilio_debug_H

#include <RemoteDebug.h> //https://github.com/JoaoLopesF/RemoteDebug
extern RemoteDebug Debug;

#ifndef DEBUGMODE
#define DEBUG_print(fmt, ...)
#define DEBUG_println(a)
#define ERROR_print(fmt, ...)
#define ERROR_println(a)
#define DEBUGSTART(a)
#else
#define DEBUG_print(fmt, ...)                                                                                                                                                      \
  if (Debug.isActive(Debug.DEBUG)) Debug.printf("%0lu " fmt, millis() / 1000, ##__VA_ARGS__)
#define DEBUG_println(a)                                                                                                                                                           \
  if (Debug.isActive(Debug.DEBUG)) Debug.printf("%0lu %s\n", millis() / 1000, a)
#define ERROR_print(fmt, ...)                                                                                                                                                      \
  if (Debug.isActive(Debug.ERROR)) Debug.printf("%0lu " fmt, millis() / 1000, ##__VA_ARGS__)
#define ERROR_println(a)                                                                                                                                                           \
  if (Debug.isActive(Debug.ERROR)) Debug.printf("%0lu %s\n", millis() / 1000, a)
#define DEBUGSTART(a) Serial.begin(a);
#endif

#endif // _debug_H