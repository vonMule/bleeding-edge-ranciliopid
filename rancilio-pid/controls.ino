/********************************************************
 * Perfect Coffee PID
 * https://github.com/medlor/bleeding-edge-ranciliopid
 *****************************************************/
#include "controls.h"
#include "rancilio-pid.h"

void splitStringBySeperator(char* source, char seperator, char** resultLeft, char** resultRight) {
  char* separator1 = strchr(source, seperator);
  if (separator1 != 0) {
    *separator1 = 0;
    *resultLeft = source;
    ++separator1;
    *resultRight = separator1;
  } else {
    snprintf(debugLine, sizeof(debugLine), "Cannot split line=%s by seperator=%c", source, seperator);
    ERROR_println(debugLine);
    resultLeft = NULL;
    resultRight = NULL;
  }
}
void splitStringBySeperator(char* source, char seperator, int* resultLeft, int* resultRight) {
  char* separator1 = strchr(source, seperator);
  if (separator1 != 0) {
    *separator1 = 0;
    *resultLeft = atoi(source);
    ++separator1;
    *resultRight = atoi(separator1);
  } else {
    snprintf(debugLine, sizeof(debugLine), "Cannot split line=%s by seperator=%c", source, seperator);
    ERROR_println(debugLine);
  }
}
void splitStringBySeperator(char* source, char seperator, int* resultLeft, char** resultRight) {
  char* separator1 = strchr(source, seperator);
  if (separator1 != 0) {
    *separator1 = 0;
    *resultLeft = atoi(source);
    ++separator1;
    *resultRight = separator1;
  } else {
    snprintf(debugLine, sizeof(debugLine), "Cannot split line=%s by seperator=%c", source, seperator);
    ERROR_println(debugLine);
  }
}
void splitStringBySeperator(char* source, char seperator, char** resultLeft, float* resultRight) {
  char* separator1 = strchr(source, seperator);
  if (separator1 != 0) {
    *separator1 = 0;
    *resultLeft = source;
    ++separator1;
    *resultRight = atof(separator1);
  } else {
    snprintf(debugLine, sizeof(debugLine), "Cannot split line=%s by seperator=%c", source, seperator);
    ERROR_println(debugLine);
  }
}


controlMap* parseControlsConfig() {
  controlMap* controlsConfig = NULL;
  controlMap* lastControlMap = NULL;

  // Read each controlsConfigPin pair
  // eg. CONTROLS_CONFIG
  // "17:analog:toggle|90-130:BREWING;270-320:STEAMING;390-420:HOTWATER#16:digital:trigger|0-0:BREWING;#"
  char* controlsConfigDefine = (char*)calloc(1, strlen(CONTROLS_CONFIG) + 1);
  strncpy(controlsConfigDefine, CONTROLS_CONFIG, strlen(CONTROLS_CONFIG));
  char* controlsConfigBlock;
  while ((controlsConfigBlock = strtok_r(controlsConfigDefine, "#", &controlsConfigDefine)) != NULL) {
    // snprintf(debugLine, sizeof(debugLine), "controlsConfigBlock=%s",
    // controlsConfigBlock); DEBUG_println(debugLine);

    // Split the controlsConfigBlock in controlsConfigPinDefinition +
    // actionMappings
    char* controlsConfigBlockDefinition = NULL;
    char* controlsConfigActionMappings = NULL;
    splitStringBySeperator(controlsConfigBlock, '|', &controlsConfigBlockDefinition, &controlsConfigActionMappings);
    if (!controlsConfigBlockDefinition || !controlsConfigActionMappings) { break; }

    // snprintf(debugLine, sizeof(debugLine), "controlsConfigBlockDefinition=%s
    // controlsConfigActionMappings=%s", controlsConfigBlockDefinition,
    // controlsConfigActionMappings); DEBUG_println(debugLine);

    int controlsConfigGpio = -1;
    char* controlsConfigPortMode = NULL;
    char* controlsConfigPortType = NULL;
    char* controlsConfigType = NULL;
    splitStringBySeperator(controlsConfigBlock, ':', &controlsConfigGpio, &controlsConfigPortMode);
    splitStringBySeperator(controlsConfigPortMode, ':', &controlsConfigPortMode, &controlsConfigPortType);
    splitStringBySeperator(controlsConfigPortType, ':', &controlsConfigPortType, &controlsConfigType);
    // snprintf(debugLine, sizeof(debugLine), "Gpio=%i PortMode=%s PortType=%s
    // Type=%s", controlsConfigGpio, controlsConfigPortMode,
    // controlsConfigPortType, controlsConfigType); DEBUG_println(debugLine);

    char* p = controlsConfigActionMappings;
    char* actionMap;
    controlMap* nextControlMap = NULL;
    while ((actionMap = strtok_r(p, ";", &p)) != NULL) {
      // snprintf(debugLine, sizeof(debugLine), "controlsConfigActionMap=%s", actionMap); DEBUG_println(debugLine);

      char* actionMapRange = NULL;
      char* actionMapAction = NULL;
      splitStringBySeperator(actionMap, ':', &actionMapRange, &actionMapAction);
      // snprintf(debugLine, sizeof(debugLine), "actionMapRange=%s
      // actionMapAction=%s", actionMapRange, actionMapAction);
      // DEBUG_println(debugLine);

      int lowerBoundary = -1;
      int upperBoundary = -1;
      splitStringBySeperator(actionMapRange, '-', &lowerBoundary, &upperBoundary);
      // snprintf(debugLine, sizeof(debugLine), "lowerBoundary=%u
      // upperBoundary=%u", lowerBoundary, upperBoundary);
      // DEBUG_println(debugLine);
      if (convertActionToDefine(actionMapAction) >= MAX_NUM_ACTIONS) {
        ERROR_println("More actions defined as allowed in MAX_NUM_ACTIONS");
        if (!nextControlMap) nextControlMap->action = UNDEFINED_ACTION;
        continue;
      }
      if (controlsConfigGpio > MAX_NUM_GPIO) {
        ERROR_println("More gpio used as allowed in MAX_NUM_GPIO");
        if (!nextControlMap) nextControlMap->gpio = 0;
        continue;
      }
      if (!strcmp(controlsConfigType, "toggle") && convertActionToDefine(actionMapAction) == MENU) {
        ERROR_println("Action MENU is not supported for \"toggle\" types.");
        continue;
      }
      nextControlMap = (controlMap*)calloc(1, sizeof(struct controlMap));
      nextControlMap->gpio = controlsConfigGpio;
      nextControlMap->portMode = controlsConfigPortMode;
      nextControlMap->portType = controlsConfigPortType;
      nextControlMap->type = controlsConfigType;
      nextControlMap->lowerBoundary = lowerBoundary;
      nextControlMap->upperBoundary = upperBoundary;
      nextControlMap->action = convertActionToDefine(actionMapAction);
      nextControlMap->value = -1;
      nextControlMap->gpioCheck = NULL;
      if (controlsConfig == NULL && lastControlMap == NULL) {
        controlsConfig = nextControlMap;
        lastControlMap = controlsConfig;
      } else {
        lastControlMap->nextControlMap = nextControlMap;
        lastControlMap = nextControlMap;
      }
    }
  }
  return controlsConfig;
}

void printControlsConfig(controlMap* controlsConfig) {
  if (!controlsConfig) {
    DEBUG_println("controlsConfig is empty");
    return;
  }
  DEBUG_println("ControlsConfig:");
  controlMap* ptr = controlsConfig;
  do {
    snprintf(debugLine, sizeof(debugLine), "%2i(%15s,%7s,%7s): %4u-%-4u -> %s", ptr->gpio, ptr->portMode, ptr->portType, ptr->type, ptr->lowerBoundary, ptr->upperBoundary,
        convertDefineToAction(ptr->action));
    DEBUG_println(debugLine);
  } while ((ptr = ptr->nextControlMap) != NULL);
}

void printMultiToggleConfig() {
  DEBUG_println("Multi-toggle:");
  int multi_toggle_1[] = MULTI_TOGGLE_1_GPIOS;
  int multi_toggle_2[] = MULTI_TOGGLE_2_GPIOS;
  int multi_toggle_3[] = MULTI_TOGGLE_3_GPIOS;
  int* multi_toggle;
  int len;
  char* action;
  for (int element = 1; element <= 3; element++) {
    switch (element) {
      case 1: {
        multi_toggle = multi_toggle_1;
        len = (int)sizeof(multi_toggle_1) / sizeof(multi_toggle_1[0]);
        action = (char*)MULTI_TOGGLE_1_ACTION;
        break;
      }
      case 2: {
        multi_toggle = multi_toggle_2;
        len = (int)sizeof(multi_toggle_2) / sizeof(multi_toggle_2[0]);
        action = (char*)MULTI_TOGGLE_2_ACTION;
        break;
      }
      case 3: {
        // multi_toggle[] = MULTI_TOGGLE_3_GPIOS;
        multi_toggle = multi_toggle_3;
        len = (int)sizeof(multi_toggle_3) / sizeof(multi_toggle_3[0]);
        action = (char*)MULTI_TOGGLE_3_ACTION;
        break;
      }
    }
    switch (len) {
      case 0: {
        break;
      }
      case 2: {
        DEBUG_print("%d: %d + %d               -> %s\n", element, multi_toggle[0], multi_toggle[1], action);
        break;
      }
      case 3: {
        DEBUG_print("%d: %d + %d + %d          -> %s\n", element, multi_toggle[0], multi_toggle[1], multi_toggle[2], action);
        break;
      }
      case 4: {
        DEBUG_print("%d: %d + %d + %d + %d     -> %s\n", element, multi_toggle[0], multi_toggle[1], multi_toggle[2], multi_toggle[3], action);
        break;
      }
      case 5: {
        DEBUG_print("%d: %d + %d + %d + %d + %d -> %s\n", element, multi_toggle[0], multi_toggle[1], multi_toggle[2], multi_toggle[3], multi_toggle[4], action);
        break;
      }
      default: {
        ERROR_print("Cannot parse multi-toggle defined in userConfig.h\n");
        break;
      }
    }
  }
}

menuMap* parseMenuConfig() {
  menuMap* menuConfig = NULL;
  menuMap* lastMenuMap = NULL;
  // Read each menuConfigPin pair
  // eg. MENU_CONFIG "CONFIG:SETPOINT:0.5:#CONFIG:BREWTIME:0.5#CONFIG:PREINFUSION:0.1#CONFIG:PREINFUSION_PAUSE:0.1#ACTION:SLEEPING:1"
  char* menuConfigDefine = (char*)calloc(1, strlen(MENU_CONFIG) + 1);
  strncpy(menuConfigDefine, MENU_CONFIG, strlen(MENU_CONFIG));
  char* menuConfigBlock;
  while ((menuConfigBlock = strtok_r(menuConfigDefine, "#", &menuConfigDefine)) != NULL) {
    // snprintf(debugLine, sizeof(debugLine), "controlsConfigBlock=%s", controlsConfigBlock); DEBUG_println(debugLine);
    char* menuConfigType = NULL;   // CONFIG | ACTION
    char* menuConfigItem = NULL;   // CONFIG=<Support setting to change> | ACTION=<Action_Name>
    float menuConfigValueStep = -1;  // CONFIG=<step-size> | ACTION =<1|0>
    splitStringBySeperator(menuConfigBlock, ':', &menuConfigType, &menuConfigItem);
    splitStringBySeperator(menuConfigItem, ':', &menuConfigItem, &menuConfigValueStep);
    if (!menuConfigType || !menuConfigValueStep) { break; }
    //snprintf(debugLine, sizeof(debugLine), "menuConfigType=%s menuConfigItem=%s menuConfigValueStep=%0.2f", menuConfigType, menuConfigItem, menuConfigValueStep); 
    //DEBUG_println(debugLine);
    menuMap* nextMenuMap = NULL;
    nextMenuMap = (menuMap*)calloc(1, sizeof(struct menuMap));
    nextMenuMap->type = menuConfigType;
    nextMenuMap->item = menuConfigItem;
    nextMenuMap->valueStep = menuConfigValueStep;
    nextMenuMap->action = strcmp(menuConfigType, "ACTION") == 0 ? convertActionToDefine(menuConfigItem): UNDEFINED_ACTION;
    nextMenuMap->value = NULL;
    nextMenuMap->unit = NULL;
    nextMenuMap->value = (menuMapValue*)calloc(1, sizeof(struct menuMapValue));
    if (strcmp(nextMenuMap->item, "SETPOINT") == 0) {
        nextMenuMap->unit = (char*)"C";
        nextMenuMap->value->type = (char*)"float";
        nextMenuMap->value->ptr = &activeSetPoint;
        nextMenuMap->value->is_double_ptr = true;
    } else if (strcmp(nextMenuMap->item, "BREWTIME") == 0) {
        nextMenuMap->unit = (char*)"s";
        nextMenuMap->value->type = (char*)"float";
        nextMenuMap->value->ptr = &activeBrewTime;
        nextMenuMap->value->is_double_ptr = true;
    } else if (strcmp(nextMenuMap->item, "PREINFUSION") == 0) {
        nextMenuMap->unit = (char*)"s";
        nextMenuMap->value->type = (char*)"float";
        nextMenuMap->value->ptr = &activePreinfusion;
        nextMenuMap->value->is_double_ptr = true;
    } else if (strcmp(nextMenuMap->item, "PREINFUSION_PAUSE") == 0) {
        nextMenuMap->unit = (char*)"s";
        nextMenuMap->value->type = (char*)"float";
        nextMenuMap->value->ptr = &activePreinfusionPause;
        nextMenuMap->value->is_double_ptr = true;
    } else if (strcmp(nextMenuMap->item, "SLEEPING") == 0) {
        nextMenuMap->value->type = (char*)"bool";
        nextMenuMap->value->ptr = &sleeping;
        nextMenuMap->value->is_double_ptr = false;
    } else if (strcmp(nextMenuMap->item, "CLEANING") == 0) {
        nextMenuMap->value->type = (char*)"bool";
        nextMenuMap->value->ptr = &cleaning;
        nextMenuMap->value->is_double_ptr = false;
    } else if (strcmp(nextMenuMap->item, "PID_ON") == 0) {
        nextMenuMap->value->type = (char*)"bool";
        nextMenuMap->value->ptr = &pidON;
        nextMenuMap->value->is_double_ptr = false;
    } else if (strcmp(nextMenuMap->item, "SETPOINT_STEAM") == 0) {
        nextMenuMap->unit = (char*)"C";
        nextMenuMap->value->type = (char*)"float";
        nextMenuMap->value->ptr = &setPointSteam;
        nextMenuMap->value->is_double_ptr = false;
    } else if (strcmp(nextMenuMap->item, "PROFILE") == 0) {
        nextMenuMap->unit = (char*)"";
        nextMenuMap->value->type = (char*)"int";
        nextMenuMap->value->ptr = &profile;
        nextMenuMap->value->is_double_ptr = false;
    } else if (strcmp(nextMenuMap->item, "BREWTIME_END_DETECTION") == 0) {
        nextMenuMap->value->type = (char*)"bool";
        nextMenuMap->value->ptr = &activeBrewTimeEndDetection;
        nextMenuMap->value->is_double_ptr = true;
    } else if (strcmp(nextMenuMap->item, "SCALE_SENSOR_WEIGHT_SETPOINT") == 0) {
        nextMenuMap->unit = (char*)"g";
        nextMenuMap->value->type = (char*)"float";
        nextMenuMap->value->ptr = &activeScaleSensorWeightSetPoint;
        nextMenuMap->value->is_double_ptr = true;
    } else {
      ERROR_print("menuConfig defined %s but is not a supported setting\n", nextMenuMap->item);
      continue;
    }
    if (menuConfig == NULL && lastMenuMap == NULL) {
      menuConfig = nextMenuMap;
      lastMenuMap = menuConfig;
    } else {
      lastMenuMap->nextMenuMap = nextMenuMap;
      lastMenuMap = nextMenuMap;
    }
  }
  return menuConfig;
}

void printMenuConfig(menuMap* menuConfig) {
  if (!menuConfig) {
    DEBUG_println("menuConfig is empty");
    return;
  }
  DEBUG_println("MenuConfig:");
  menuMap* ptr = menuConfig;
  int counter = 0;
  do {
    counter++;
    if (strcmp(ptr->type, "ACTION") == 0) {
      snprintf(debugLine, sizeof(debugLine), "%d: %s %s", counter, ptr->type, convertDefineToAction(ptr->action));
    } else {
      snprintf(debugLine, sizeof(debugLine), "%d: %s %s in steps of %0.2f", counter, ptr->type, ptr->item, ptr->valueStep);
    }
    DEBUG_println(debugLine);
  } while ((ptr = ptr->nextMenuMap) != NULL);
}

menuMap* getMenuConfigPosition(menuMap* menuConfig, unsigned int menuPosition) {
  //menuPosition starts with 1 !
  if (!menuConfig) {
    DEBUG_println("menuConfig is empty");
    return NULL;
  }
  menuMap* ptr = menuConfig;
  unsigned int counter = 1;
  do {
    if (counter == menuPosition) return ptr;
    counter++;
  } while ((ptr = ptr->nextMenuMap) != NULL);
  return NULL;
}

const char* convertDefineToVariable(char *str) {
  if (!strcmp(str, "SETPOINT")) return "activeSetPoint";
  if (!strcmp(str, "SETPOINTSTEAM")) return "setPointSteam";
  if (!strcmp(str, "BREWTIME")) return "activeBrewTime";
  if (!strcmp(str, "PREINFUSION")) return "activePreinfusion";
  if (!strcmp(str, "PREINFUSION_PAUSE")) return "activePreinfusionPause";
  if (!strcmp(str, "PID_ON")) return "pidON";
  if (!strcmp(str, "PROFILE")) return "profile";
  if (!strcmp(str, "BREWTIME_END_DETECTION")) return "activeBrewTimeEndDetection"; 
  if (!strcmp(str, "SCALE_SENSOR_WEIGHT_SETPOINT")) return "activeScaleSensorWeightSetPoint"; 
  ERROR_print("convertDefineToVariable(%s) not supported", str);
  return NULL;
}

const char* convertDefineToReadAbleVariable(char *str) {
  if (!strcmp(str, "SETPOINT")) return "Brew Temperature";
  if (!strcmp(str, "SETPOINTSTEAM")) return "Steaming Temp.";
  if (!strcmp(str, "BREWTIME")) return "Brew Duration";
  if (!strcmp(str, "PREINFUSION")) return "Preinfusion Duration";
  if (!strcmp(str, "PREINFUSION_PAUSE")) return "Preinf. Pause";
  if (!strcmp(str, "PID_ON")) return "Heating";
  if (!strcmp(str, "SLEEPING")) return "Sleeping";
  if (!strcmp(str, "STEAMING")) return "Steaming";
  if (!strcmp(str, "CLEANING")) return "Cleaning";
  if (!strcmp(str, "PROFILE")) return "Profile";
  if (!strcmp(str, "BREWTIME_END_DETECTION")) return "Brew by Weight";
  if (!strcmp(str, "SCALE_SENSOR_WEIGHT_SETPOINT")) return "Brew Weight";
  return str;
}

void menuConfigPositionModifyByStep(menuMap* menuConfigPosition, bool increase = 1) {
  if (strcmp(menuConfigPosition->type, "ACTION") == 0) {
    actionController(menuConfigPosition->action, increase);
  } else {
    char* setting = (char*)convertDefineToVariable(menuConfigPosition->item);
    static char settingMQTTSet[50];
    if (setting) snprintf(settingMQTTSet, sizeof(settingMQTTSet), "%s/set", setting);
    if (!strcmp(menuConfigPosition->value->type, "bool")) {
      bool value;
      if (menuConfigPosition->value->is_double_ptr) {
        value = **((int**)menuConfigPosition->value->ptr);
      } else {
        value = *((int*)menuConfigPosition->value->ptr);
      }
      if (increase) {
        value = 1;
      } else {
        value = 0;
      }
      if (setting) { 
        mqttPublish(setting, number2string(value));
        mqttPublish(settingMQTTSet, number2string(value));
      }
    } else if (!strcmp(menuConfigPosition->value->type, "float")) {
      float value;
      if (menuConfigPosition->value->is_double_ptr) {
        value = **((float**)menuConfigPosition->value->ptr);
      } else {
        value = *((float*)menuConfigPosition->value->ptr);
      }
      if (increase) {
        value += float(menuConfigPosition->valueStep);
      } else {
        value -= float(menuConfigPosition->valueStep);
      }
      if (setting) {
        mqttPublish(setting, number2string(value));
        mqttPublish(settingMQTTSet, number2string(value));
      }
    } else {
      int value;
      if (menuConfigPosition->value->is_double_ptr) {
        value = **((int**)menuConfigPosition->value->ptr);
      } else {
        value = *((int*)menuConfigPosition->value->ptr);
      }
      if (increase) {
        value += int(menuConfigPosition->valueStep);
      } else {
        value -= int(menuConfigPosition->valueStep);
      }
      if (setting) {
        mqttPublish(setting, number2string(value));
        mqttPublish(settingMQTTSet, number2string(value));
      }
    }
    if (setting) {
      eepromForceSync = millis();
      blynkSave(setting);
    }
  }
}

void publishActions() {
  char topicAction[32];
  for (int i = 0; i < MAX_NUM_ACTIONS; i++) {
    char* action = convertDefineToAction(i);
    if (strcmp(action, "UNDEFINED_ACTION") != 0) {
      sprintf(topicAction, "actions/%s", action);
      mqttPublish(topicAction, int2string(actionState[i]));
    }
  }
}

void configureControlsHardware(controlMap* controlsConfig) {
  if (!controlsConfig) {
    DEBUG_println("controlsConfig is empty");
    return;
  }
  controlMap* ptr = controlsConfig;
  int port = -1;
  do {
    if (ptr->gpio == port) { continue; }
    port = ptr->gpio;
    snprintf(debugLine, sizeof(debugLine), "Set Hardware GPIO %2i to %s", ptr->gpio, ptr->portMode);
    DEBUG_println(debugLine);
    if (strcmp(ptr->portType, "analog") == 0) {
      pinMode(ptr->gpio, convertPortModeToDefine(ptr->portMode));
    } else {
      //pinMode(ptr->gpio, convertPortModeToDefine(ptr->portMode));
      if (ptr->gpioCheck == NULL) ptr->gpioCheck = new GpioCheck(ptr->gpio, ptr->portMode, DEBOUNCE_DIGITAL_GPIO);
      if (ptr->gpioCheck != NULL) {
        (ptr->gpioCheck)->begin();
        DEBUG_print("configureControlsHardware(): gpio=%d running\n", ptr->gpio);
      } else {
        ERROR_print("configureControlsHardware(): gpio=%d error\n", ptr->gpio);
      }
    }
  } while ((ptr = ptr->nextControlMap) != NULL);
}

void debugControlHardware(controlMap* controlsConfig) {
  if (!controlsConfig) {
    DEBUG_println("controlsConfig is empty");
    return;
  }
  controlMap* ptr = controlsConfig;
  int port = -1;
  int valueRead = -1;
  DEBUG_println("-------------------------------------");
  do {
    if (ptr->gpio == port) { continue; }
    port = ptr->gpio;
    if (strcmp(ptr->portType, "analog") == 0) {
      valueRead = analogRead(ptr->gpio);
    } else {
      valueRead = digitalRead(ptr->gpio);
    }
    snprintf(debugLine, sizeof(debugLine), "GPIO %2i: %d", ptr->gpio, valueRead);
    DEBUG_println(debugLine);
  } while ((ptr = ptr->nextControlMap) != NULL);
}

int convertActionToDefine(char* action) {
  if (!strcmp(action, "UNDEFINED_ACTION")) {
    return UNDEFINED_ACTION;
  } else if (!strcmp(action, "BREWING")) {
    return BREWING;
  } else if (!strcmp(action, "HOTWATER")) {
    return HOTWATER;
  } else if (!strcmp(action, "STEAMING")) {
    return STEAMING;
  } else if (!strcmp(action, "CLEANING")) {
    return CLEANING;
  } else if (!strcmp(action, "MENU")) {
    return MENU;
  } else if (!strcmp(action, "MENU_INC")) {
    return MENU_INC;
  } else if (!strcmp(action, "MENU_DEC")) {
    return MENU_DEC;
  } else if (!strcmp(action, "SLEEPING")) {
    return SLEEPING;
  }
  return UNDEFINED_ACTION;
}

int convertPortModeToDefine(char* portMode) {
  if (!strcmp(portMode, "INPUT_PULLUP")) {
    return INPUT_PULLUP;
  }
#ifdef ESP32
  else if (!strcmp(portMode, "INPUT_PULLDOWN")) {
    return INPUT_PULLDOWN;
  }
#endif
  return INPUT;
}

char* convertDefineToAction(int action) {
  if (action == UNDEFINED_ACTION) {
    return (char*)"UNDEFINED_ACTION";
  } else if (action == BREWING) {
    return (char*)"BREWING";
  } else if (action == HOTWATER) {
    return (char*)"HOTWATER";
  } else if (action == STEAMING) {
    return (char*)"STEAMING";
  } else if (action == CLEANING) {
    return (char*)"CLEANING";
  } else if (action == MENU) {
    return (char*)"MENU";
  } else if (action == MENU_INC) {
    return (char*)"MENU_INC";
  } else if (action == MENU_DEC) {
    return (char*)"MENU_DEC";
  } else if (action == SLEEPING) {
    return (char*)"SLEEPING";
  }
  return (char*)"UNDEFINED_ACTION";
}

int getActionOfControl(controlMap* controlsConfig, int port, int value) {
  if (!controlsConfig) { return UNDEFINED_ACTION; }
  controlMap* ptr = controlsConfig;
  do {
    if (port == ptr->gpio) {
      if (value >= ptr->lowerBoundary && value <= ptr->upperBoundary) { return ptr->action; }
    }
  } while ((ptr = ptr->nextControlMap) != NULL);
  return UNDEFINED_ACTION;
}

void actionController(int action, int newState) { actionController(action, newState, true, true); }

void actionController(int action, int newState, bool publishActionMqtt) { actionController(action, newState, publishActionMqtt, true); }

void actionController(int action, int newState, bool publishAction, bool publishActionBlynk) {
  if (action == UNDEFINED_ACTION) { return; }
  // newState := if newState >=0 set value to newState. If newState == -1 ->
  // toggle between 0/1
  int oldState = actionState[action];
  if (newState == -1) {
    if (oldState <= 1) {
      newState = oldState == 0 ? 1 : 0;
    } else {
      snprintf(debugLine, sizeof(debugLine),
          "Error: actionController(%s, %d) tries to inverse an illegal "
          "value %d",
          convertDefineToAction(action), newState, oldState);
      ERROR_println(debugLine);
      newState = 0; // fallback/safe-guard
    }
  }
  // call special helper functions when state changes
  // actionState logic should remain in actionController() function and not the
  // helper functions
  if (newState != oldState) {
    //snprintf(debugLine, sizeof(debugLine), "actionController: Setting %s %d->%d", convertDefineToAction(action), oldState, newState);
    //DEBUG_println(debugLine);
    userActivity = millis();
    if (action == HOTWATER) {
      actionController(BREWING, 0);
      actionController(SLEEPING, 0);
      actionState[action] = newState;
      hotwaterAction(newState);
      if (publishAction) mqttPublish((char*)"actions/HOTWATER", int2string(newState));
    } else if (action == BREWING) {
      actionController(STEAMING, 0);
      actionController(HOTWATER, 0);
      actionController(SLEEPING, 0);
      actionState[action] = newState;
      brewingAction(newState);
      actionPublish((char*)"actions/BREWING", 101, newState, publishAction, publishActionBlynk);
    } else if (action == STEAMING) {
      actionController(BREWING, 0);
      actionController(CLEANING, 0);
      actionController(SLEEPING, 0);
      actionState[action] = newState;
      steamingAction(newState);
      actionPublish((char*)"actions/STEAMING", 103, newState, publishAction, publishActionBlynk);
    } else if (action == CLEANING) {
      actionController(BREWING, 0);
      actionController(HOTWATER, 0);
      actionController(STEAMING, 0);
      actionController(SLEEPING, 0);
      actionState[action] = newState;
      cleaningAction(newState);
      actionPublish((char*)"actions/CLEANING", 107, newState, publishAction, publishActionBlynk);
    } else if (action == SLEEPING) {
      actionController(BREWING, 0);
      actionController(CLEANING, 0);
      actionController(HOTWATER, 0);
      actionController(STEAMING, 0);
      actionState[action] = newState;
      sleepingAction(newState);
      actionPublish((char*)"actions/SLEEPING", 110, newState, publishAction, publishActionBlynk);
    } else if (action == MENU) {
      actionState[action] = newState;
      menuAction(newState);
      //actionPublish((char*)"actions/MENU", 110, newState, publishAction, publishActionBlynk);
    } else if (action == MENU_INC) {
      actionState[action] = newState;
      menuIncAction(newState);
      //actionPublish((char*)"actions/MENUINC", 110, newState, publishAction, publishActionBlynk);
    } else if (action == MENU_DEC) {
      actionState[action] = newState;
      menuDecAction(newState);
      //actionPublish((char*)"actions/MENUDEC", 110, newState, publishAction, publishActionBlynk);
    }
    //snprintf(debugLine, sizeof(debugLine), "actionController: Completed %s %d->%d", convertDefineToAction(action), oldState, actionState[action]);
    //DEBUG_println(debugLine);
  } else {
    // snprintf(debugLine, sizeof(debugLine), "action=%s state=%d (old=%d)
    // NOCHANGE", convertDefineToAction(action), actionState[action], oldState);
    // DEBUG_println(debugLine);
  }
}

void actionPublish(char* mqtt_topic, unsigned int blynk_vpin, int newState, bool publishActionMQTT, bool publishActionBlynk) {
  // TODO add mapping table of action/setting to mqtt-topic&blynk_vpin
  if (publishActionMQTT) mqttPublish(mqtt_topic, int2string(newState));
#if (BLYNK_ENABLE == 1)
  if (publishActionBlynk) Blynk.virtualWrite(blynk_vpin, newState);
#endif
}

bool checkArrayInArray(int a[], int sizeof_a, int b[], int sizeof_b) {
  // DEBUG_print("checkArrayInArray() a[]=%d,%d,%d\n", a[0], a[1], a[2]);
  // DEBUG_print("checkArrayInArray() b[]=%d,%d,%d\n", b[0], b[1], b[2]);
  if (sizeof_a == 0) return false;
  bool found = false;
  for (int i1 = 0; i1 < sizeof_a; i1++) {
    // DEBUG_print("checkArrayInArray() i1: %d (sizeof=%d)\n", a[i1], sizeof_a);
    if (a[i1] == -1) break;
    found = false;
    for (int i2 = 0; i2 < sizeof_b; i2++) {
      // DEBUG_print("checkArrayInArray(i1=%d) i2=%d\n", a[i1], b[i2]);
      if (a[i1] == b[i2]) {
        found = true;
        break;
      }
      if (b[i2] == -1) break;
    }
    // DEBUG_print("checkArrayInArray(): i2 loop next item A\n");
    if (found != true) return false;
    // DEBUG_print("checkArrayInArray(): i2 loop next item B\n");
  }
  // DEBUG_print("checkArrayInArray(): true\n");
  return true;
}

int checkMultiToggle() {
  int multi_toggle_1[] = MULTI_TOGGLE_1_GPIOS;
  int multi_toggle_2[] = MULTI_TOGGLE_2_GPIOS;
  int multi_toggle_3[] = MULTI_TOGGLE_3_GPIOS;
  controlMap* ptr = controlsConfig;
  int portRead = -1;
  const int max_toggles = 5; // limit to max 5 toggles
  int gpio_active[max_toggles] = { -1, -1, -1, -1, -1 };
  int pos = 0;
  do {
    if (ptr->gpio == portRead) { continue; }
    if (strcmp(ptr->portType, "digital") != 0) { continue; }
    if (strcmp(ptr->type, "toggle") != 0) { continue; }
    portRead = ptr->gpio;
    // DEBUG_print("reading gpio %d (pos=%d)\n", portRead, (pos));
    if (gpioLastAction[portRead] != UNDEFINED_ACTION) {
      gpio_active[pos] = portRead;
      // DEBUG_print("found action gpio %d (pos=%d)\n", portRead, (pos));
      pos++;
    }
  } while ((ptr = ptr->nextControlMap) != NULL);
  // snprintf(debugLine, sizeof(debugLine), "gpio_active=%d,%d,%d,%d,%d",
  // gpio_active[0], gpio_active[1], gpio_active[2], gpio_active[3],
  // gpio_active[4]); DEBUG_println(debugLine); snprintf(debugLine,
  // sizeof(debugLine), "multi_toggle_1=%d,%d,%d", multi_toggle_1[0],
  // multi_toggle_1[1], multi_toggle_1[2]); DEBUG_println(debugLine);
  if (checkArrayInArray(multi_toggle_1, (int)(sizeof(multi_toggle_1) / sizeof(multi_toggle_1[0])), gpio_active, max_toggles)) {
    return convertActionToDefine((char*)MULTI_TOGGLE_1_ACTION);
  }
  if (checkArrayInArray(multi_toggle_2, (int)(sizeof(multi_toggle_2) / sizeof(multi_toggle_2[0])), gpio_active, max_toggles)) {
    return convertActionToDefine((char*)MULTI_TOGGLE_2_ACTION);
  }
  if (checkArrayInArray(multi_toggle_3, (int)(sizeof(multi_toggle_3) / sizeof(multi_toggle_3[0])), gpio_active, max_toggles)) {
    return convertActionToDefine((char*)MULTI_TOGGLE_3_ACTION);
  }
  return UNDEFINED_ACTION;
}

void checkControls(controlMap* controlsConfig) {
  if (!controlsConfig) { return; }
  unsigned long aktuelleZeit = millis();
  if (aktuelleZeit >= previousCheckControls + FREQUENCYCHECKCONTROLS) {
    previousCheckControls = aktuelleZeit;
    controlMap* ptr = controlsConfig;
    int currentMultiAction = -1;
    int currentAction = -1;
    int portRead = -1;
    int valueRead = -1;
    int valueReadMultiSample = -1;
    do {
      if (ptr->gpio == portRead) { continue; }
      portRead = ptr->gpio;

      if (strcmp(ptr->portType, "analog") == 0) {
        valueRead = analogRead(portRead); // analog pin
        // process button press
        currentAction = getActionOfControl(controlsConfig, portRead, valueRead);
#if (DEBOUNCE_ANALOG_GPIO != 0)
        //(ESP32 ADC seems not to be stable) multisample read to ignore outlier / flappings / debouncing
        if (((strcmp(ptr->type, "trigger") ==0) && currentAction != UNDEFINED_ACTION ) || ((strcmp(ptr->type, "toggle") == 0) && currentAction != gpioLastAction[portRead])) {
          delay(DEBOUNCE_ANALOG_GPIO);
          valueReadMultiSample = analogRead(portRead);
          if (fabs(valueRead - valueReadMultiSample) >= 300 || currentAction != getActionOfControl(controlsConfig, portRead, valueReadMultiSample)) {
            snprintf(debugLine, sizeof(debugLine), "GPIO %d: IGNORED %s (%d, %d)", portRead, convertDefineToAction(currentAction), valueRead, valueReadMultiSample);
            ERROR_println(debugLine);
            valueRead = -1;
            currentAction = UNDEFINED_ACTION;
            continue; // ignore samples this time. ok?
          }
        }
#endif
      } else {  //digital pin
        if (ptr->gpioCheck != NULL) {
          unsigned char counter = (ptr->gpioCheck)->getCounter(); //counter >0 when gpio interrupt had fired
          //if (counter >0) { DEBUG_print("GPIO=%d: COUNTER=%u\n", ptr->gpio, counter); }
          if (counter==0) { continue; }
        }
        valueRead = digitalRead(portRead); // digital pin

        // process button press
        currentAction = getActionOfControl(controlsConfig, portRead, valueRead);
#if (DEBOUNCE_DIGITAL_GPIO != 0)
        // multisample read to surpress flapping/debouncing
        if (((strcmp(ptr->type, "trigger") == 0) && currentAction != UNDEFINED_ACTION) || ((strcmp(ptr->type, "toggle") == 0) && currentAction != gpioLastAction[portRead])) {
          delay(DEBOUNCE_DIGITAL_GPIO);  //despite the internal debounce functionality in GpioCheck.h, we still debounce on a higher level yet again
          valueReadMultiSample = digitalRead(portRead);
          if (valueRead != valueReadMultiSample) {
            snprintf(debugLine, sizeof(debugLine), "GPIO %d: IGNORED %s (%d, %d)", portRead, convertDefineToAction(currentAction), valueRead, valueReadMultiSample);
            ERROR_println(debugLine);
            valueRead = -1;
            currentAction = UNDEFINED_ACTION;
            continue; // ignore samples this time.
          }
        }
#endif
      }

      if (strcmp(ptr->type, "trigger") == 0) {
        if (currentAction == UNDEFINED_ACTION) { // no boundaries match ->
          // button not pressed anymore
          if (gpioLastAction[portRead] != UNDEFINED_ACTION) {
            // snprintf(debugLine, sizeof(debugLine), "GPIO %d: action=%s
            // state=released", portRead,
            // convertDefineToAction(gpioLastAction[portRead]));
            // DEBUG_println(debugLine);
          }
          gpioLastAction[portRead] = UNDEFINED_ACTION;
        } else if ((gpioLastAction[portRead] == UNDEFINED_ACTION) && (currentAction != gpioLastAction[portRead])) {
          // snprintf(debugLine, sizeof(debugLine), "GPIO %d: action=%s
          // valueRead=%d", portRead, convertDefineToAction(currentAction),
          // valueRead); DEBUG_println(debugLine);
          gpioLastAction[portRead] = currentAction;
          actionController(currentAction, -1);
        } else { // if button is still pressed
                 // snprintf(debugLine, sizeof(debugLine), "GPIO %d: action=%s
                 // state=unchanged", ptr->gpio, convertDefineToAction(currentAction));
                 // DEBUG_println(debugLine);
        }
      } else if (strcmp(ptr->type, "toggle") == 0) {
        if (currentAction != gpioLastAction[portRead]) {
          DEBUG_print("GPIO %d: %s -> %s\n", portRead, convertDefineToAction(gpioLastAction[portRead]),
              convertDefineToAction(currentAction)); // TODO comment this line
        }
        if (currentAction == UNDEFINED_ACTION) { // no boundaries match ->
          // toggle is in "off" position
          if (gpioLastAction[portRead] != currentAction) { // disable old
            // action
            // check multi actions (makes sense only for toggle)
            int lastMultiAction = checkMultiToggle();
            int lastGpioLastAction = gpioLastAction[portRead];
            gpioLastAction[portRead] = currentAction;
            currentMultiAction = checkMultiToggle();
            // DEBUG_print("GPIO %d: lastMultiAction=%s
            // currentMultiAction=%s\n", portRead,
            // convertDefineToAction(lastMultiAction),
            // convertDefineToAction(currentMultiAction) );
            if (currentMultiAction != UNDEFINED_ACTION) {
              if (currentMultiAction == lastMultiAction) {
                // if currentMultiAction already is active, then execute regular
                // single-toggle action
                actionController(lastGpioLastAction, 0);
              } else {
                // activate new multiAction determined by toggled gpio
                actionController(currentMultiAction, 1);
              }
            } else {
              if (lastMultiAction == UNDEFINED_ACTION) {
                // if no multiAction then just turn gpio action off
                actionController(lastGpioLastAction, 0);
              } else {
                actionController(lastMultiAction, 0);
              }
            }
          }
        } else if ((gpioLastAction[portRead] == UNDEFINED_ACTION) && (currentAction != UNDEFINED_ACTION)) {
          gpioLastAction[portRead] = currentAction;
          // check multi actions (makes sense only for toggle)
          currentMultiAction = checkMultiToggle();
          if (currentMultiAction != UNDEFINED_ACTION) {
            // if currentMultiAction already is active, then execute regular
            // single-toggle action
            if (actionState[currentMultiAction]) {
              actionController(currentAction, 1);
            } else {
              actionController(currentMultiAction, 1);
            }
          } else {
            actionController(currentAction, 1);
          }
        } else if ((gpioLastAction[portRead] != UNDEFINED_ACTION) && (currentAction != gpioLastAction[portRead])) { // another action if same port
          // is triggered
          actionController(gpioLastAction[portRead], 0);
          gpioLastAction[portRead] = currentAction;
          // check multi actions (makes sense only for toggle)
          currentMultiAction = checkMultiToggle();
          if (currentMultiAction != UNDEFINED_ACTION) {
            actionController(currentMultiAction, 1);
          } else {
            actionController(currentAction, 1);
          }
          // actionController(currentAction, 1);
        } else if (currentAction == gpioLastAction[portRead]) {
          // snprintf(debugLine, sizeof(debugLine), "GPIO %d: action=%s
          // state=unchanged", portRead, convertDefineToAction(currentAction));
          // DEBUG_println(debugLine);
        }
      } else {
        snprintf(debugLine, sizeof(debugLine), "type (%s) unknown?", ptr->type);
        DEBUG_println(debugLine);
      }
    } while ((ptr = ptr->nextControlMap) != NULL);
  }
}

/********************************************************
 * helper functions to execute actions
 *********************************************************/
void brewingAction(int state) {
  if (OnlyPID) {
    if (brewDetection == 1) {
      brewing = state; // brewing should only be set if the maschine is in
                       // reality brewing!
      setGpioAction(BREWING, state);
    }
    return;
  }
  simulatedBrewSwitch = state;
  DEBUG_print("running brewingAction(%d)\n", state);
  setGpioAction(BREWING, state);
}

void hotwaterAction(int state) {
  // Function switches the pump on to dispense hot water
  if (OnlyPID) return;
  if (state == 1) {
    digitalWrite(pinRelayPumpe, relayON);
  } else if (state == 0) {
    digitalWrite(pinRelayPumpe, relayOFF);
  }
  DEBUG_print("running hotwaterAction(%d)\n", state);
  setGpioAction(HOTWATER, state);
}

void steamingAction(int state) {
  steaming = state;
  DEBUG_print("running steamingAction(%d)\n", state);
  setGpioAction(STEAMING, state);
}

void cleaningAction(int state) {
  if (OnlyPID) return;
  // CLEANING is a special state which is not reflected in activeState because
  // it is overarching to all activeStates.
  cleaning = state;
  DEBUG_print("running cleaningAction(%d)\n", state);
  setGpioAction(CLEANING, state);
  bPID.SetAutoTune(state);
  bPID.SetMode(!state);
}

void sleepingAction(int state) {
  userActivitySavedOnForcedSleeping = userActivity;
  sleeping = state;
  DEBUG_print("running sleepingAction(%d)\n", state);
  if (!state) {
    lastBrewEnd = millis();
    // reset some special auto-tuning variables
    MaschineColdstartRunOnce = false;
    steadyPowerOffsetModified = steadyPowerOffset;
  }
}

void menuAction(int state) {
  //show menu by increasing menuPos++
  menuPosition++;
  if (!getMenuConfigPosition(menuConfig, menuPosition)) {
    menuPosition=0;
    return;
  }
  DEBUG_print("running menuAction(%d)\n", menuPosition);
  //refresh menu timer
  previousTimerMenuCheck = millis();  
  sleeping = 0;
}

void menuIncAction(int state) {
  menuMap* menuConfigPosition = getMenuConfigPosition(menuConfig, menuPosition);
  if (!menuConfigPosition) return;
  DEBUG_print("running menuIncAction(%d)\n", menuPosition);
  //refresh menu timer
  previousTimerMenuCheck = millis();
  menuConfigPositionModifyByStep(menuConfigPosition, true);
}

void menuDecAction(int state) {
  menuMap* menuConfigPosition = getMenuConfigPosition(menuConfig, menuPosition);
  if (!menuConfigPosition) return;
  DEBUG_print("running menuDecAction(%d)\n", menuPosition);
  //refresh menu timer
  previousTimerMenuCheck = millis();
  menuConfigPositionModifyByStep(menuConfigPosition, false); 
}
