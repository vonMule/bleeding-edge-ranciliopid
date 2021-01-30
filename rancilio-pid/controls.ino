unsigned long previousCheckControls = 0;
#define FREQUENCYCHECKCONTROLS 1000  // TOBIAS: change to 50 or 200?

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

    //controlMap controlConfigA0[count+1]; // = controlMap[count+1];
    snprintf(debugline, sizeof(debugline), "inside parseControlsConfig():");
    DEBUG_println(debugline);

    // Read each controlsConfigPin pair 
    // eg. A0:toggle|245-260:brewCoffee;525-540:steam;802-815:hotwater;#"
    char* controlsConfigDefine = (char*) malloc(strlen(CONTROLS_CONFIG));
    strncpy(controlsConfigDefine, CONTROLS_CONFIG, strlen(CONTROLS_CONFIG));
    char* controlsConfigBlock;
    while ((controlsConfigBlock = strtok_r(controlsConfigDefine, "#", &controlsConfigDefine)) != NULL) {
        int item = 0;
        int lowerBoundary = 0;
        int upperBoundary = 0;
        snprintf(debugline, sizeof(debugline), "controlsConfigBlock=%s", controlsConfigBlock);
        DEBUG_println(debugline);

        // Split the controlsConfigBlock in controlsConfigPinDefinition + actionMappings
        char* controlsConfigBlockDefinition = NULL;
        char* controlsConfigActionMappings = NULL;
        splitStringBySeperator(controlsConfigBlock, '|', &controlsConfigBlockDefinition, &controlsConfigActionMappings);
        if (!controlsConfigBlockDefinition || !controlsConfigActionMappings) { break; }

        snprintf(debugline, sizeof(debugline), "controlsConfigBlockDefinition=%s controlsConfigActionMappings=%s", controlsConfigBlockDefinition, controlsConfigActionMappings);
        DEBUG_println(debugline);

        int controlsConfigPort = -1;
        char* controlsConfigPortType = NULL;
        char* controlsConfigType = NULL;
        splitStringBySeperator(controlsConfigBlock, ':', &controlsConfigPort, &controlsConfigPortType);
        splitStringBySeperator(controlsConfigPortType, ':', &controlsConfigPortType, &controlsConfigType);
        snprintf(debugline, sizeof(debugline), "controlsConfigPort=%i controlsConfigPortType=%s controlsConfigType=%s", controlsConfigPort, controlsConfigPortType, controlsConfigType);
        DEBUG_println(debugline);

        char* p = controlsConfigActionMappings;
        char* actionMap;
        controlMap* nextControlMap;
        while ((actionMap = strtok_r(p, ";", &p)) != NULL) {
          snprintf(debugline, sizeof(debugline), "controlsConfigActionMap=%s", actionMap);
          DEBUG_println(debugline);

          char* actionMapRange = NULL;
          char* actionMapAction = NULL;
          splitStringBySeperator(actionMap, ':', &actionMapRange, &actionMapAction);
          snprintf(debugline, sizeof(debugline), "actionMapRange=%s actionMapAction=%s", actionMapRange, actionMapAction);
          DEBUG_println(debugline);

          int lowerBoundary = -1;
          int upperBoundary = -1;
          splitStringBySeperator(actionMapRange, '-', &lowerBoundary, &upperBoundary);
          snprintf(debugline, sizeof(debugline), "lowerBoundary=%u upperBoundary=%u", lowerBoundary, upperBoundary);
          DEBUG_println(debugline);
          if (convertActionToDefine(actionMapAction) >= MAX_NUM_ACTIONS) {
            ERROR_println("More actions defined as allowed in MAX_NUM_ACTIONS");
            nextControlMap->action = UNDEFINED_ACTION;
            continue;
          }
          if (controlsConfigPort >= MAX_NUM_GPIO) {
            ERROR_println("More gpio used as allowed in MAX_NUM_GPIO");
            nextControlMap->port = 0;
            continue;
          }
          nextControlMap = (controlMap*) calloc(1, sizeof (struct controlMap));
          nextControlMap->port = controlsConfigPort;
          nextControlMap->portType = controlsConfigPortType; 
          nextControlMap->type = controlsConfigType; 
          nextControlMap->lowerBoundary = lowerBoundary; 
          nextControlMap->upperBoundary = upperBoundary; 
          nextControlMap->action = convertActionToDefine(actionMapAction);

          nextControlMap->value = -1;
          
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
  controlMap* ptr = controlsConfig;
  do {
    snprintf(debugline, sizeof(debugline), "%2i(%7s,%7s): %4u-%-4u -> %s", ptr->port, ptr->portType, ptr->type, ptr->lowerBoundary, ptr->upperBoundary, convertDefineToAction(ptr->action));
    DEBUG_println(debugline);
  } while ((ptr = ptr->nextControlMap) != NULL);
}


void configureControlsHardware(controlMap* controlsConfig) {
  if (!controlsConfig) {
    DEBUG_println("controlsConfig is empty");
    return;
  }
  controlMap* ptr = controlsConfig;
  int port = -1;
  do {
    if (ptr->port == port) {continue;}
    port = ptr->port;
    snprintf(debugline, sizeof(debugline), "Set Hardware GPIO %2i to INPUT", ptr->port);
    DEBUG_println(debugline);
    if ( strcmp(ptr->portType, "analog") == 0) {
      pinMode(ptr->port, INPUT);
      //add stuff here?
    } else {
      pinMode(ptr->port, INPUT);
    }
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
  return UNDEFINED_ACTION;
}


char* convertDefineToAction(int action) {
  if (action == UNDEFINED_ACTION) { return "UNDEFINED_ACTION";}
  else if (action == BREWING) { return "BREWING";}
  else if (action == HOTWATER) { return "HOTWATER";}
  else if (action == STEAMING) { return "STEAMING";}
  else if (action == CLEANING) { return "CLEANING";}
  else if (action == TEMP_INC) { return "TEMP_INC";}
  else if (action == TEMP_DEC) { return "TEMP_DEC";}
  return "UNDEFINED_ACTION";
}


int getActionOfControl(controlMap* controlsConfig, int port, int value) {
  if (!controlsConfig) { return UNDEFINED_ACTION; }
  controlMap* ptr = controlsConfig;
  do {
    if (port == ptr->port) {
      if (value >= ptr->lowerBoundary && value <= ptr->upperBoundary) {
        return ptr->action;
      }
    }
  } while ((ptr = ptr->nextControlMap) != NULL);
  return UNDEFINED_ACTION;
}

// maschine states used by state-maschine: brewing, steaming, ... -> actionState
// GPIO state to save current and lastAction
//int gpio_16_lastAction = UNDEFINED_ACTION;
//int gpio_17_lastAction = UNDEFINED_ACTION;


void actionController(int action, int newState) {  
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
  actionState[action] = newState;
  
  snprintf(debugline, sizeof(debugline), "action=%s state=%d (old=%d)", convertDefineToAction(action), actionState[action], oldState);
  DEBUG_println(debugline);
  //call special helper functions when state changes
  //actionState logic should remain in actionController() function and not the helper functions
  if (newState != oldState) {
    if (action == HOTWATER) { actionController(BREWING, 0); actionController(CLEANING, 0); hotwaterAction(newState); }
    else if (action == BREWING) { actionController(HOTWATER, 0); actionController(CLEANING, 0); brewingAction(newState); }
    else if (action == STEAMING) { actionController(BREWING, 0); actionController(CLEANING, 0); steamingAction(newState); }
    else if (action == CLEANING) { actionController(BREWING, 0); actionController(STEAMING, 0); actionController(HOTWATER, 0); cleaningAction(newState); }
  }
}



/*
void* executeActionOfControl(controlMap* controlsConfig, int port, int value) {
  if (!controlsConfig) { return; }
  controlMap* ptr = controlsConfig;
  char *gpio_17_new_action = NULL;
  do {
    if (port == ptr->port) {
      if (value >= ptr->lowerBoundary && value <= ptr->upperBoundary) {
        if (ptr->type == "trigger") {
          sample_action = sample_action == 0 ? 1 : 0; 
        }
        else if (ptr->type == "toggle") {
          //sample_action = sample_action == 0 ? 1 : 0; 
          gpio_17_new_action = ptr->action;
          
        } else {
          snprintf(debugline, sizeof(debugline), "type (%s) unknown?", ptr->type);
          DEBUG_println(debugline);
        }
      }
    }
  } while ((ptr = ptr->nextControlMap) != NULL);
  if (gpio_17_new_state == NULL) {
    snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=off", ptr->port, gpio_17_new_action);
    DEBUG_println(debugline);
  }
  else if (strcmp(gpio_17_new_action, gpio_17_latest_state) != 0) {
    snprintf(debugline, sizeof(debugline), "GPIO %d: no change in state", ptr->port);
    DEBUG_println(debugline);
  } else {
    snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=unchanged", ptr->port, gpio_17_new_action);
    DEBUG_println(debugline);
  }
  return;
}
*/


void checkControls(controlMap* controlsConfig) {
  if (!controlsConfig) {return;}
  unsigned long aktuelleZeit = millis();
  if ( aktuelleZeit >= previousCheckControls + FREQUENCYCHECKCONTROLS ) {
    previousCheckControls = aktuelleZeit;
    DEBUG_println("inside checkControls()");
    
    controlMap* ptr = controlsConfig;
    int currentAction = -1;
    int portRead = -1;
    int valueRead = -1;
    do {
      if (ptr->port == portRead) {continue;}
      portRead = ptr->port;
      
      if ( strcmp(ptr->portType, "analog") == 0) {
        valueRead = analogRead(ptr->port);  //analog pin
      } else {
        valueRead = digitalRead(ptr->port); //digital pin
      }

      //process button press
      currentAction = getActionOfControl(controlsConfig, portRead, valueRead);
      if (strcmp(ptr->type, "trigger") == 0) {
        if (currentAction == UNDEFINED_ACTION) {  //no boundaries match -> button not pressed anymore
          if (gpioLastAction[ptr->port] != UNDEFINED_ACTION) {
            //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=released", ptr->port, convertDefineToAction(gpio_16_lastAction));
            //DEBUG_println(debugline);
          }
          gpioLastAction[ptr->port] = UNDEFINED_ACTION;
        } else if ((gpioLastAction[ptr->port] == UNDEFINED_ACTION) && (currentAction != gpioLastAction[ptr->port])) {
          gpioLastAction[ptr->port] = currentAction;
          actionController(currentAction, -1);
        } else {  //if button is still pressed
          //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=unchanged", ptr->port, convertDefineToAction(currentAction));
          //DEBUG_println(debugline);
        }
        
      } else if (strcmp(ptr->type, "toggle") == 0) {
        if (currentAction == UNDEFINED_ACTION) {  //no boundaries match -> toggle is in "off" position
          if (gpioLastAction[ptr->port] != UNDEFINED_ACTION) {  //disable old action
            actionController(gpioLastAction[ptr->port], 0);
          }
          gpioLastAction[ptr->port] = UNDEFINED_ACTION;
        } else if ((gpioLastAction[ptr->port] == UNDEFINED_ACTION) && (currentAction != UNDEFINED_ACTION)) {
          gpioLastAction[ptr->port] = currentAction;
          actionController(currentAction, 1);
        } else if ((gpioLastAction[ptr->port] != UNDEFINED_ACTION) && (currentAction != gpioLastAction[ptr->port])) {  //another action if same port is triggered
          //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=%d", ptr->port, convertDefineToAction(gpio_17_lastAction), 0);  //disable old action
          //DEBUG_println(debugline);
          actionController(gpioLastAction[ptr->port], 0);
          gpioLastAction[ptr->port] = currentAction;
          actionController(currentAction, 1);
        } else if (currentAction == gpioLastAction[ptr->port]) {
          //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=unchanged", ptr->port, convertDefineToAction(currentAction));
          //DEBUG_println(debugline);
        }
       
      } else {
        snprintf(debugline, sizeof(debugline), "type (%s) unknown?", ptr->type);
        DEBUG_println(debugline);
      }
      
      
    } while ((ptr = ptr->nextControlMap) != NULL);
  }
}


/*
void checkControlsOLD(controlMap* controlsConfig) {
  if (!controlsConfig) { return; }
  unsigned long aktuelleZeit = millis();
  if ( aktuelleZeit >= previousCheckControls + FREQUENCYCHECKCONTROLS ) {
    previousCheckControls = aktuelleZeit;

    DEBUG_println("inside checkControls()");

    int portRead = -1;
    int valueRead = -1;
    controlMap* ptr = controlsConfig;
    do {
      //snprintf(debugline, sizeof(debugline), "%2i(%7s %7s): %4i-%-4i -> %s", ptr->port, ptr->portType, ptr->type, ptr->lowerBoundary, ptr->upperBoundary, ptr->action);
      //DEBUG_println(debugline);
      
      //if (ptr->port == -1) break;
      if (ptr->port != portRead) {  //other gpio in loop found
        portRead = ptr->port;
        
        if ( strcmp(ptr->portType, "analog") == 0) {
          valueRead = analogRead(ptr->port);  //analog pin

          if (ptr->value >=0 && valueRead >= ptr->value +10 || valueRead <= ptr->value -10) {
            snprintf(debugline, sizeof(debugline), "DETECTED %s: gpio=%i readValue=%i (oldValue=%i)", getActionOfControl(controlsConfig, ptr->port, valueRead), ptr->port, valueRead, ptr->value);
            DEBUG_println(debugline);
            executeActionOfControl(controlsConfig, ptr->port, valueRead);
          }
          ptr->value = valueRead;
          
        } else {
          valueRead = digitalRead(ptr->port); //digital pin

          if (ptr->value >=0 && valueRead != ptr->value) {
            snprintf(debugline, sizeof(debugline), "DETECTED %s: gpio=%i readValue=%i (oldValue=%i)", getActionOfControl(controlsConfig, ptr->port, valueRead), ptr->port, valueRead, ptr->value);
            DEBUG_println(debugline);
            executeActionOfControl(controlsConfig, ptr->port, valueRead);
          }
          ptr->value = valueRead;
        }
        
      } else {  //still the same gpio port in loop
        //generate event TOBIAS YYY2
        if (valueRead >= ptr->value +10 || valueRead <= ptr->value -10) {
           snprintf(debugline, sizeof(debugline), "DETECTED: gpio=%i readValue=%i (oldValue=%i)", ptr->port, valueRead, ptr->value);
            DEBUG_println(debugline);
          ptr->value = valueRead;
        }
      }
      
    } while ((ptr = ptr->nextControlMap) != NULL);

  }
}
*/

void brewingAction(int state) {
  brewing = state;
}

void hotwaterAction(int state) {
  // Function switches the pump on to dispense hot water
  if (state == 1 && digitalRead(pinRelayPumpe) == relayOFF) {
    digitalWrite(pinRelayPumpe, relayON);
    DEBUG_print("pump relay: on.\n");
  } else if (state == 0 && digitalRead(pinRelayPumpe) == relayON) {
    digitalWrite(pinRelayPumpe, relayOFF);
    DEBUG_print("pump relay: off.\n");
  }
}

void steamingAction(int state) { 
  // Function resets the setpoint-, P-, I-, D-, Values
  // of the PID Controller to generate steam.
  // Sets active state to State 6.
  /*
  if (*activeSetPoint != setPointSteam) {  ///YYY1
    activeSetPoint = &setPointSteam;
    DEBUG_print("set activeSetPoint: %0.2f Steam\n", setPointSteam);
  }
  */
  steaming = state; 
  /*
  if (activeState != 6) {
    activeState = 6;  //YYY  //not ideal to have state maschine status changed externally
    DEBUG_print("set activeState: 6 Steam\n");
  }
  */
}

void cleaningAction(int state) { 
  int cleaning = state; 
}
