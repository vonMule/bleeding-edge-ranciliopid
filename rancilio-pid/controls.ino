/********************************************************
 * BLEEDING EDGE RANCILIO-PID.
 * https://github.com/medlor/bleeding-edge-ranciliopid
 *****************************************************/

void splitStringBySeperator(char* source, char seperator, char** resultLeft, char** resultRight) {
    char* separator1 = strchr(source, seperator);
    if (separator1 != 0)
    {
        *separator1 =  0;
        *resultLeft = source;
        ++separator1;
        *resultRight = separator1;
    } else {
      snprintf(debugline, sizeof(debugline), "Cannot split line=%s by seperator=%c", source, seperator);
      ERROR_println(debugline);
      resultLeft = NULL;
      resultRight = NULL;
    }
}

void splitStringBySeperator(char* source, char seperator, int* resultLeft, int* resultRight) {
    char* separator1 = strchr(source, seperator);
    if (separator1 != 0)
    {
        *separator1 =  0;
        *resultLeft = atoi(source);
        ++separator1;
        *resultRight = atoi(separator1);
    } else {
      snprintf(debugline, sizeof(debugline), "Cannot split line=%s by seperator=%c", source, seperator);
      ERROR_println(debugline);
    }
}
void splitStringBySeperator(char* source, char seperator, int* resultLeft, char** resultRight) {
    char* separator1 = strchr(source, seperator);
    if (separator1 != 0)
    {
        *separator1 =  0;
        *resultLeft = atoi(source);
        ++separator1;
        *resultRight = separator1;
    } else {
      snprintf(debugline, sizeof(debugline), "Cannot split line=%s by seperator=%c", source, seperator);
      ERROR_println(debugline);
    }
}

controlMap* parseControlsConfig() {
    controlMap* controlsConfig = NULL;
    controlMap* lastControlMap = NULL;

    DEBUG_println(debugline);

    // Read each controlsConfigPin pair
    // eg. CONTROLS_CONFIG "17:analog:toggle|90-130:BREWING;270-320:STEAMING;390-420:HOTWATER#16:digital:trigger|0-0:BREWING;#"
    char* controlsConfigDefine = (char*) calloc(1, strlen(CONTROLS_CONFIG)+1);
    strncpy(controlsConfigDefine, CONTROLS_CONFIG, strlen(CONTROLS_CONFIG));
    char* controlsConfigBlock;
    while ((controlsConfigBlock = strtok_r(controlsConfigDefine, "#", &controlsConfigDefine)) != NULL) {
        int item = 0;
        int lowerBoundary = 0;
        int upperBoundary = 0;
        //snprintf(debugline, sizeof(debugline), "controlsConfigBlock=%s", controlsConfigBlock);
        //DEBUG_println(debugline);

        // Split the controlsConfigBlock in controlsConfigPinDefinition + actionMappings
        char* controlsConfigBlockDefinition = NULL;
        char* controlsConfigActionMappings = NULL;
        splitStringBySeperator(controlsConfigBlock, '|', &controlsConfigBlockDefinition, &controlsConfigActionMappings);
        if (!controlsConfigBlockDefinition || !controlsConfigActionMappings) { break; }

        //snprintf(debugline, sizeof(debugline), "controlsConfigBlockDefinition=%s controlsConfigActionMappings=%s", controlsConfigBlockDefinition, controlsConfigActionMappings);
        //DEBUG_println(debugline);

        int controlsConfigGpio = -1;
        char* controlsConfigPortMode = NULL;
        char* controlsConfigPortType = NULL;
        char* controlsConfigType = NULL;
        splitStringBySeperator(controlsConfigBlock, ':', &controlsConfigGpio, &controlsConfigPortMode);
        splitStringBySeperator(controlsConfigPortMode, ':', &controlsConfigPortMode, &controlsConfigPortType);
        splitStringBySeperator(controlsConfigPortType, ':', &controlsConfigPortType, &controlsConfigType);
        //snprintf(debugline, sizeof(debugline), "Gpio=%i PortMode=%s PortType=%s Type=%s", controlsConfigGpio, controlsConfigPortMode, controlsConfigPortType, controlsConfigType);
        //DEBUG_println(debugline);

        char* p = controlsConfigActionMappings;
        char* actionMap;
        controlMap* nextControlMap;
        while ((actionMap = strtok_r(p, ";", &p)) != NULL) {
          //snprintf(debugline, sizeof(debugline), "controlsConfigActionMap=%s", actionMap);
          //DEBUG_println(debugline);

          char* actionMapRange = NULL;
          char* actionMapAction = NULL;
          splitStringBySeperator(actionMap, ':', &actionMapRange, &actionMapAction);
          //snprintf(debugline, sizeof(debugline), "actionMapRange=%s actionMapAction=%s", actionMapRange, actionMapAction);
          //DEBUG_println(debugline);

          int lowerBoundary = -1;
          int upperBoundary = -1;
          splitStringBySeperator(actionMapRange, '-', &lowerBoundary, &upperBoundary);
          //snprintf(debugline, sizeof(debugline), "lowerBoundary=%u upperBoundary=%u", lowerBoundary, upperBoundary);
          //DEBUG_println(debugline);
          if (convertActionToDefine(actionMapAction) >= MAX_NUM_ACTIONS) {
            ERROR_println("More actions defined as allowed in MAX_NUM_ACTIONS");
            nextControlMap->action = UNDEFINED_ACTION;
            continue;
          }
          if (controlsConfigGpio >= MAX_NUM_GPIO) {
            ERROR_println("More gpio used as allowed in MAX_NUM_GPIO");
            nextControlMap->gpio = 0;
            continue;
          }
          nextControlMap = (controlMap*) calloc(1, sizeof (struct controlMap));
          nextControlMap->gpio          = controlsConfigGpio;
          nextControlMap->portMode      = controlsConfigPortMode;
          nextControlMap->portType      = controlsConfigPortType;
          nextControlMap->type          = controlsConfigType;
          nextControlMap->lowerBoundary = lowerBoundary;
          nextControlMap->upperBoundary = upperBoundary;
          nextControlMap->action        = convertActionToDefine(actionMapAction);
          nextControlMap->value         = -1;
          if (controlsConfig == NULL && lastControlMap == NULL ) {
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
    snprintf(debugline, sizeof(debugline), "%2i(%15s,%7s,%7s): %4u-%-4u -> %s", ptr->gpio, ptr->portMode, ptr->portType, ptr->type, ptr->lowerBoundary, ptr->upperBoundary, convertDefineToAction(ptr->action));
    DEBUG_println(debugline);
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
  for (int element=1; element <=3; element++) {
    switch (element) {
      case 1:
      {
        multi_toggle = multi_toggle_1;
        len = (int) sizeof(multi_toggle_1)/sizeof(multi_toggle_1[0]);
        action = (char*) MULTI_TOGGLE_1_ACTION;
        break;
      }
      case 2:
      {
        multi_toggle = multi_toggle_2;
        len = (int) sizeof(multi_toggle_2)/sizeof(multi_toggle_2[0]);
        action = (char*) MULTI_TOGGLE_2_ACTION;
        break;
      }
      case 3:
      {
        //multi_toggle[] = MULTI_TOGGLE_3_GPIOS;
        multi_toggle = multi_toggle_3;
        len = (int) sizeof(multi_toggle_3)/sizeof(multi_toggle_3[0]);
        action = (char*) MULTI_TOGGLE_3_ACTION;
        break;
      }
    }
    switch (len) {
      case 0:
      {
        break;
      }
      case 2:
      {
        DEBUG_print("%d: %d + %d               -> %s\n", element, multi_toggle[0], multi_toggle[1],action);
        break;
      }
      case 3:
      {
        DEBUG_print("%d: %d + %d + %d          -> %s\n", element, multi_toggle[0], multi_toggle[1], multi_toggle[2], action);
        break;
      }
      case 4:
      {
        DEBUG_print("%d: %d + %d + %d + %d     -> %s\n", element, multi_toggle[0], multi_toggle[1], multi_toggle[2], multi_toggle[3], action);
        break;
      }
      case 5:
      {
        DEBUG_print("%d: %d + %d + %d + %d + %d -> %s\n", element, multi_toggle[0], multi_toggle[1],  multi_toggle[2], multi_toggle[3], multi_toggle[4], action);
        break;
      }
      default:
      {
        ERROR_print("Cannot parse multi-toggle defined in userConfig.h\n");
        break;
      }
    }
  }
}
  

void publishActions() {
  char topicAction[32];
  for (int i=0; i< MAX_NUM_ACTIONS; i++) {
    char *action = convertDefineToAction(i);
    if ( strcmp(action, "UNDEFINED_ACTION") != 0) {
      sprintf(topicAction, "actions/%s", action);
      mqtt_publish(topicAction, int2string(actionState[i]));
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
    if (ptr->gpio == port) {continue;}
    port = ptr->gpio;
    snprintf(debugline, sizeof(debugline), "Set Hardware GPIO %2i to %s", ptr->gpio, ptr->portMode);
    DEBUG_println(debugline);
    if ( strcmp(ptr->portType, "analog") == 0) {
      pinMode(ptr->gpio, convertPortModeToDefine(ptr->portMode));
    } else {;
      pinMode(ptr->gpio, convertPortModeToDefine(ptr->portMode));
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
    if (ptr->gpio == port) {continue;}
    port = ptr->gpio;
    if ( strcmp(ptr->portType, "analog") == 0) {
      valueRead = analogRead(ptr->gpio);
    } else {
      valueRead = digitalRead(ptr->gpio);
    }
    snprintf(debugline, sizeof(debugline), "GPIO %2i: %d", ptr->gpio, valueRead);
    DEBUG_println(debugline);
  } while ((ptr = ptr->nextControlMap) != NULL);
}


int convertActionToDefine(char* action) {
  if (!strcmp(action, "UNDEFINED_ACTION")) { return UNDEFINED_ACTION;}
  else if (!strcmp(action, "BREWING")) { return BREWING;}
  else if (!strcmp(action, "HOTWATER")) { return HOTWATER;}
  else if (!strcmp(action, "STEAMING")) { return STEAMING;}
  else if (!strcmp(action, "CLEANING")) { return CLEANING;}
  else if (!strcmp(action, "TEMP_INC")) { return TEMP_INC;}
  else if (!strcmp(action, "TEMP_DEC")) { return TEMP_DEC;}
  else if (!strcmp(action, "SLEEPING")) { return SLEEPING;}
  return UNDEFINED_ACTION;
}


int convertPortModeToDefine(char* portMode) {
  if (!strcmp(portMode, "INPUT_PULLUP")) { return INPUT_PULLUP;}
  #ifdef ESP32
  else if (!strcmp(portMode, "INPUT_PULLDOWN")) { return INPUT_PULLDOWN;}
  #endif
  return INPUT;
}


char* convertDefineToAction(int action) {
  if (action == UNDEFINED_ACTION) { return "UNDEFINED_ACTION";}
  else if (action == BREWING) { return "BREWING";}
  else if (action == HOTWATER) { return "HOTWATER";}
  else if (action == STEAMING) { return "STEAMING";}
  else if (action == CLEANING) { return "CLEANING";}
  else if (action == TEMP_INC) { return "TEMP_INC";}
  else if (action == TEMP_DEC) { return "TEMP_DEC";}
  else if (action == SLEEPING) { return "SLEEPING";}
  return "UNDEFINED_ACTION";
}


int getActionOfControl(controlMap* controlsConfig, int port, int value) {
  if (!controlsConfig) { return UNDEFINED_ACTION; }
  controlMap* ptr = controlsConfig;
  do {
    if (port == ptr->gpio) {
      if (value >= ptr->lowerBoundary && value <= ptr->upperBoundary) {
        return ptr->action;
      }
    }
  } while ((ptr = ptr->nextControlMap) != NULL);
  return UNDEFINED_ACTION;
}


void actionController(int action, int newState) {
  actionController(action, newState, true);
}


void actionController(int action, int newState, bool publishAction) {
  if (action == UNDEFINED_ACTION) { return; }
  //newState := if newState >=0 set value to newState. If newState == -1 -> toggle between 0/1
  int oldState = actionState[action];
  if (newState == -1) {
    if (oldState <=1) {
      newState = oldState == 0 ? 1 : 0;
    } else {
      snprintf(debugline, sizeof(debugline), "Error: actionController(action=%s state=%d) wants to trigger a not triggerable stored value=%d", convertDefineToAction(action), newState, oldState);
      DEBUG_println(debugline);
      newState = 0; //fallback/safe-guard
    }
  }
  //call special helper functions when state changes
  //actionState logic should remain in actionController() function and not the helper functions
  if (newState != oldState) {
    userActivity = millis();
    if (action == HOTWATER) { actionController(BREWING, 0); actionController(SLEEPING, 0); actionState[action] = newState; hotwaterAction(newState); if (publishAction) mqtt_publish("actions/HOTWATER", int2string(newState));}
    else if (action == BREWING) { actionController(STEAMING, 0); actionController(HOTWATER, 0); actionController(SLEEPING, 0); actionState[action] = newState; brewingAction(newState); if (publishAction) mqtt_publish("actions/BREWING", int2string(newState)); Blynk.virtualWrite(V101, newState);}
    else if (action == STEAMING) { actionController(BREWING, 0); actionController(CLEANING, 0); actionController(SLEEPING, 0); actionState[action] = newState; steamingAction(newState); if (publishAction) mqtt_publish("actions/STEAMING", int2string(newState)); Blynk.virtualWrite(V103, newState);}
    else if (action == CLEANING) { actionController(BREWING, 0); actionController(HOTWATER, 0); actionController(STEAMING, 0); actionController(SLEEPING, 0); actionState[action] = newState; cleaningAction(newState); if (publishAction) mqtt_publish("actions/CLEANING", int2string(newState)); Blynk.virtualWrite(V107, newState);}
    else if (action == SLEEPING) { actionController(BREWING, 0); actionController(CLEANING, 0); actionController(HOTWATER, 0); actionController(STEAMING, 0); actionState[action] = newState; sleepingAction(newState); if (publishAction) mqtt_publish("actions/SLEEPING", int2string(newState)); Blynk.virtualWrite(V110, newState);}
    snprintf(debugline, sizeof(debugline), "action=%s state=%d (old=%d)", convertDefineToAction(action), actionState[action], oldState);
    DEBUG_println(debugline);
  }
  //snprintf(debugline, sizeof(debugline), "action=%s state=%d (old=%d)", convertDefineToAction(action), actionState[action], oldState);
  //DEBUG_println(debugline);
}

boolean checkArrayInArray(int a[], int sizeof_a, int b[], int sizeof_b) {
  //DEBUG_print("checkArrayInArray() a[]=%d,%d,%d\n", a[0], a[1], a[2]);
  //DEBUG_print("checkArrayInArray() b[]=%d,%d,%d\n", b[0], b[1], b[2]);
  if (sizeof_a == 0) return false;
  boolean found = false;
  for (int i1=0; i1 < sizeof_a; i1++) {
    //DEBUG_print("checkArrayInArray() i1: %d (sizeof=%d)\n", a[i1], sizeof_a);
    if (a[i1] == -1) break;
    found = false;
    for (int i2=0; i2 < sizeof_b; i2++) {
      //DEBUG_print("checkArrayInArray(i1=%d) i2=%d\n", a[i1], b[i2]);
      if (a[i1] == b[i2]) { found = true; break; }
      if (b[i2] == -1) break;
    }
    //DEBUG_print("checkArrayInArray(): i2 loop next item A\n");
    if (found != true) return false;
    //DEBUG_print("checkArrayInArray(): i2 loop next item B\n");
  }
  //DEBUG_print("checkArrayInArray(): true\n");
  return true;
}

int checkMultiToggle() {
  int multi_toggle_1[] = MULTI_TOGGLE_1_GPIOS;
  int multi_toggle_2[] = MULTI_TOGGLE_2_GPIOS;
  int multi_toggle_3[] = MULTI_TOGGLE_3_GPIOS;
  controlMap* ptr = controlsConfig;
  int currentAction = -1;
  int portRead = -1;
  const int max_toggles = 5; // limit to max 5 toggles
  int gpio_active[max_toggles] = {-1, -1, -1, -1, -1}; 
  int pos = 0;
  do {
      if (ptr->gpio == portRead) {continue;}
      if (strcmp(ptr->portType, "digital") != 0) {continue;}
      if (strcmp(ptr->type, "toggle") != 0) {continue;}
      portRead = ptr->gpio;
      //DEBUG_print("reading gpio %d (pos=%d)\n", portRead, (pos));
      if (gpioLastAction[portRead] != UNDEFINED_ACTION) {
        gpio_active[pos] = portRead;
        //DEBUG_print("found action gpio %d (pos=%d)\n", portRead, (pos));
        pos++;
      }
  } while ((ptr = ptr->nextControlMap) != NULL);
  //snprintf(debugline, sizeof(debugline), "gpio_active=%d,%d,%d,%d,%d", gpio_active[0], gpio_active[1], gpio_active[2], gpio_active[3], gpio_active[4]);
  //DEBUG_println(debugline);
  //snprintf(debugline, sizeof(debugline), "multi_toggle_1=%d,%d,%d", multi_toggle_1[0], multi_toggle_1[1], multi_toggle_1[2]);
  //DEBUG_println(debugline);
  if (checkArrayInArray(multi_toggle_1, (int)(sizeof(multi_toggle_1)/sizeof(multi_toggle_1[0])), gpio_active, max_toggles)) {
    return convertActionToDefine((char*) MULTI_TOGGLE_1_ACTION);
  }
  if (checkArrayInArray(multi_toggle_2, (int)(sizeof(multi_toggle_2)/sizeof(multi_toggle_2[0])), gpio_active, max_toggles)) {
    return convertActionToDefine((char*) MULTI_TOGGLE_2_ACTION);
  }
  if (checkArrayInArray(multi_toggle_3, (int)(sizeof(multi_toggle_3)/sizeof(multi_toggle_3[0])), gpio_active, max_toggles)) {
    return convertActionToDefine((char*) MULTI_TOGGLE_3_ACTION);
  }
  return UNDEFINED_ACTION;
}


void checkControls(controlMap* controlsConfig) {
  if (!controlsConfig) {return;}
  unsigned long aktuelleZeit = millis();
  if ( aktuelleZeit >= previousCheckControls + FREQUENCYCHECKCONTROLS ) {
    previousCheckControls = aktuelleZeit;
    controlMap* ptr = controlsConfig;
    int currentMultiAction = -1;
    int currentAction = -1;
    int portRead = -1;
    int valueRead = -1;
    int valueReadMultiSample = -1;
    do {
      if (ptr->gpio == portRead) {continue;}
      portRead = ptr->gpio;

      if ( strcmp(ptr->portType, "analog") == 0) {
        valueRead = analogRead(portRead);  //analog pin
        //process button press
        currentAction = getActionOfControl(controlsConfig, portRead, valueRead);
        //multisample read to ignore outlier (ESP32 ADC seems not to be stable)
        if (currentAction != UNDEFINED_ACTION) {
          valueReadMultiSample = analogRead(portRead);
          
          if (fabs(valueRead - valueReadMultiSample) >= 300) {
            snprintf(debugline, sizeof(debugline), "GPIO %d: IGNORED action=%s valueRead=%d valueReadMultiSample=%d", portRead, convertDefineToAction(currentAction), valueRead, valueReadMultiSample);
            ERROR_println(debugline);
            valueRead = -1;
            currentAction = UNDEFINED_ACTION;
          }
        }
      } else {
        valueRead = digitalRead(portRead); //digital pin
        //process button press
        currentAction = getActionOfControl(controlsConfig, portRead, valueRead);
      }

      if (strcmp(ptr->type, "trigger") == 0) {
        if (currentAction == UNDEFINED_ACTION) {  //no boundaries match -> button not pressed anymore
          if (gpioLastAction[portRead] != UNDEFINED_ACTION) {
            //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=released", portRead, convertDefineToAction(gpio_16_lastAction));
            //DEBUG_println(debugline);
          }
          gpioLastAction[portRead] = UNDEFINED_ACTION;
        } else if ((gpioLastAction[portRead] == UNDEFINED_ACTION) && (currentAction != gpioLastAction[portRead])) {
          //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s valueRead=%d", portRead, convertDefineToAction(currentAction), valueRead);
          //DEBUG_println(debugline);
          gpioLastAction[portRead] = currentAction;
          actionController(currentAction, -1);
        } else {  //if button is still pressed
          //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=unchanged", ptr->gpio, convertDefineToAction(currentAction));
          //DEBUG_println(debugline);
        }

      } else if (strcmp(ptr->type, "toggle") == 0) {
        //DEBUG_print("GPIO %d: new_action=%s cur_action=%s\n", portRead, convertDefineToAction(currentAction), convertDefineToAction(gpioLastAction[portRead]) );
        if (currentAction == UNDEFINED_ACTION) {  //no boundaries match -> toggle is in "off" position
          if (gpioLastAction[portRead] != currentAction) {  //disable old action
            //check multi actions (makes sense only for toggle)
            int lastMultiAction = checkMultiToggle();
            int lastGpioLastAction = gpioLastAction[portRead];
            gpioLastAction[portRead] = currentAction;
            currentMultiAction = checkMultiToggle();
            //DEBUG_print("GPIO %d: lastMultiAction=%s currentMultiAction=%s\n", portRead, convertDefineToAction(lastMultiAction), convertDefineToAction(currentMultiAction) );
            if ( currentMultiAction != UNDEFINED_ACTION) {
              //DEBUG_print("INSIDE XXX1\n");
              if ( currentMultiAction == lastMultiAction ) {
                //if currentMultiAction already is active, then execute regular single-toggle action
                actionController(lastGpioLastAction, 0);
              } else {
                // activate new multiAction determined by toggled gpio
                actionController(currentMultiAction, 1);
              }
            } else {
              if (lastMultiAction == UNDEFINED_ACTION) {
                //if no multiAction then just turn gpio action off
                //DEBUG_print("INSIDE XXX2\n");
                actionController(lastGpioLastAction, 0);
              } else {
                actionController(lastMultiAction, 0);
              }
            } 
          }
          
        } else if ((gpioLastAction[portRead] == UNDEFINED_ACTION) && (currentAction != UNDEFINED_ACTION)) {
          gpioLastAction[portRead] = currentAction;
          //check multi actions (makes sense only for toggle)
          currentMultiAction = checkMultiToggle();
          if (currentMultiAction != UNDEFINED_ACTION) {
            //if currentMultiAction already is active, then execute regular single-toggle action
            if (actionState[currentMultiAction]) {
              actionController(currentAction, 1);
            } else {
              actionController(currentMultiAction, 1);
            } 
          } else {
            actionController(currentAction, 1);
          }
          
        } else if ((gpioLastAction[portRead] != UNDEFINED_ACTION) && (currentAction != gpioLastAction[portRead])) {  //another action if same port is triggered
          actionController(gpioLastAction[portRead], 0);
          gpioLastAction[portRead] = currentAction;
          //check multi actions (makes sense only for toggle)
          currentMultiAction = checkMultiToggle();
          if (currentMultiAction != UNDEFINED_ACTION) {
            actionController(currentMultiAction, 1);
          } else{
            actionController(currentAction, 1);
          }          
          //actionController(currentAction, 1);
        } else if (currentAction == gpioLastAction[portRead]) {
          //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=unchanged", portRead, convertDefineToAction(currentAction));
          //DEBUG_println(debugline);
        }
        
      } else {
        snprintf(debugline, sizeof(debugline), "type (%s) unknown?", ptr->type);
        DEBUG_println(debugline);
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
       brewing = state;  //brewing should only be set if the maschine is in reality brewing!
       setGpioAction(BREWING, state);
    }
   return;
  }
  simulatedBrewSwitch = state;
  DEBUG_print("brewingAction(): simulatedBrewSwitch: %d\n", simulatedBrewSwitch);
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
  DEBUG_print("hotwaterAction(): %d\n", state);
  setGpioAction(HOTWATER, state);
}

void steamingAction(int state) {
  steaming = state;
  DEBUG_print("steamingAction(): %d\n", state);
  setGpioAction(STEAMING, state);
}

void cleaningAction(int state) {
  if (OnlyPID) return;
  //CLEANING is a special state which is not reflected in activeState because it is overarching to all activeStates.
  cleaning = state;
  DEBUG_print("cleaningAction(): %d\n", state);
  setGpioAction(CLEANING, state);
  bPID.SetAutoTune(state);
  bPID.SetMode(!state);
}

void sleepingAction(int state) {
  userActivitySavedOnForcedSleeping = userActivity;
  sleeping = state;
  DEBUG_print("sleepingAction(): %d\n", state);
  if (!state) {
    lastBrewEnd = millis();
    //reset some special auto-tuning variables
    MaschineColdstartRunOnce = false;
    steadyPowerOffsetModified = steadyPowerOffset;
  }
}
