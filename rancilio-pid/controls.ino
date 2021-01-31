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

    snprintf(debugline, sizeof(debugline), "inside parseControlsConfig():");
    DEBUG_println(debugline);

    // Read each controlsConfigPin pair 
    // eg. CONTROLS_CONFIG "17:analog:toggle|90-130:BREWING;270-320:STEAMING;390-420:HOTWATER#16:digital:trigger|0-0:BREWING;#"
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

        int controlsConfigGpio = -1;
        char* controlsConfigPortType = NULL;
        char* controlsConfigType = NULL;
        splitStringBySeperator(controlsConfigBlock, ':', &controlsConfigGpio, &controlsConfigPortType);
        splitStringBySeperator(controlsConfigPortType, ':', &controlsConfigPortType, &controlsConfigType);
        snprintf(debugline, sizeof(debugline), "controlsConfigGpio=%i controlsConfigPortType=%s controlsConfigType=%s", controlsConfigGpio, controlsConfigPortType, controlsConfigType);
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
          if (controlsConfigGpio >= MAX_NUM_GPIO) {
            ERROR_println("More gpio used as allowed in MAX_NUM_GPIO");
            nextControlMap->gpio = 0;
            continue;
          }
          nextControlMap = (controlMap*) calloc(1, sizeof (struct controlMap));
          nextControlMap->gpio = controlsConfigGpio;
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
    snprintf(debugline, sizeof(debugline), "%2i(%7s,%7s): %4u-%-4u -> %s", ptr->gpio, ptr->portType, ptr->type, ptr->lowerBoundary, ptr->upperBoundary, convertDefineToAction(ptr->action));
    DEBUG_println(debugline);
  } while ((ptr = ptr->nextControlMap) != NULL);
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
    snprintf(debugline, sizeof(debugline), "Set Hardware GPIO %2i to INPUT", ptr->gpio);
    DEBUG_println(debugline);
    if ( strcmp(ptr->portType, "analog") == 0) {
      pinMode(ptr->gpio, INPUT);
      //add stuff here?
    } else {
      pinMode(ptr->gpio, INPUT);
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
  //actionState[action] = newState;
  //snprintf(debugline, sizeof(debugline), "action=%s state=%d (old=%d)", convertDefineToAction(action), actionState[action], oldState);
  //DEBUG_println(debugline);
  //call special helper functions when state changes
  //actionState logic should remain in actionController() function and not the helper functions
  if (newState != oldState) {
    if (action == HOTWATER) { actionController(BREWING, 0); actionController(CLEANING, 0); actionState[action] = newState; hotwaterAction(newState); if (publishAction) mqtt_publish("actions/HOTWATER", int2string(newState));}
    else if (action == BREWING) { actionController(STEAMING, 0); actionController(HOTWATER, 0); actionController(CLEANING, 0); actionState[action] = newState; brewingAction(newState); if (publishAction) mqtt_publish("actions/BREWING", int2string(newState));}
    else if (action == STEAMING) { actionController(BREWING, 0); actionController(CLEANING, 0); actionState[action] = newState; steamingAction(newState); if (publishAction) mqtt_publish("actions/STEAMING", int2string(newState));}
    else if (action == CLEANING) { actionController(BREWING, 0); actionController(HOTWATER, 0); actionController(STEAMING, 0); actionState[action] = newState; cleaningAction(newState); if (publishAction) mqtt_publish("actions/CLEANING", int2string(newState));}
  }
  snprintf(debugline, sizeof(debugline), "action=%s state=%d (old=%d)", convertDefineToAction(action), actionState[action], oldState);
  DEBUG_println(debugline);
  userActivity = millis();
}


void checkControls(controlMap* controlsConfig) {
  if (!controlsConfig) {return;}
  unsigned long aktuelleZeit = millis();
  if ( aktuelleZeit >= previousCheckControls + FREQUENCYCHECKCONTROLS ) {
    previousCheckControls = aktuelleZeit;
    //DEBUG_println("inside checkControls()");
    
    controlMap* ptr = controlsConfig;
    int currentAction = -1;
    int portRead = -1;
    int valueRead = -1;
    do {
      if (ptr->gpio == portRead) {continue;}
      portRead = ptr->gpio;
      
      if ( strcmp(ptr->portType, "analog") == 0) {
        valueRead = analogRead(ptr->gpio);  //analog pin
      } else {
        valueRead = digitalRead(ptr->gpio); //digital pin
      }

      //process button press
      currentAction = getActionOfControl(controlsConfig, portRead, valueRead);
      if (strcmp(ptr->type, "trigger") == 0) {
        if (currentAction == UNDEFINED_ACTION) {  //no boundaries match -> button not pressed anymore
          if (gpioLastAction[ptr->gpio] != UNDEFINED_ACTION) {
            //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=released", ptr->gpio, convertDefineToAction(gpio_16_lastAction));
            //DEBUG_println(debugline);
          }
          gpioLastAction[ptr->gpio] = UNDEFINED_ACTION;
        } else if ((gpioLastAction[ptr->gpio] == UNDEFINED_ACTION) && (currentAction != gpioLastAction[ptr->gpio])) {
          gpioLastAction[ptr->gpio] = currentAction;
          actionController(currentAction, -1);
        } else {  //if button is still pressed
          //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=unchanged", ptr->gpio, convertDefineToAction(currentAction));
          //DEBUG_println(debugline);
        }
        
      } else if (strcmp(ptr->type, "toggle") == 0) {
        if (currentAction == UNDEFINED_ACTION) {  //no boundaries match -> toggle is in "off" position
          if (gpioLastAction[ptr->gpio] != UNDEFINED_ACTION) {  //disable old action
            actionController(gpioLastAction[ptr->gpio], 0);
          }
          gpioLastAction[ptr->gpio] = UNDEFINED_ACTION;
        } else if ((gpioLastAction[ptr->gpio] == UNDEFINED_ACTION) && (currentAction != UNDEFINED_ACTION)) {
          gpioLastAction[ptr->gpio] = currentAction;
          actionController(currentAction, 1);
        } else if ((gpioLastAction[ptr->gpio] != UNDEFINED_ACTION) && (currentAction != gpioLastAction[ptr->gpio])) {  //another action if same port is triggered
          //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=%d", ptr->gpio, convertDefineToAction(gpio_17_lastAction), 0);  //disable old action
          //DEBUG_println(debugline);
          actionController(gpioLastAction[ptr->gpio], 0);
          gpioLastAction[ptr->gpio] = currentAction;
          actionController(currentAction, 1);
        } else if (currentAction == gpioLastAction[ptr->gpio]) {
          //snprintf(debugline, sizeof(debugline), "GPIO %d: action=%s state=unchanged", ptr->gpio, convertDefineToAction(currentAction));
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
  if (OnlyPID) return;
  simulatedBrewSwitch = state;
}

void hotwaterAction(int state) {
  // Function switches the pump on to dispense hot water
  if (OnlyPID) return;
  if (state == 1) { // && digitalRead(pinRelayPumpe) == relayOFF) {
    digitalWrite(pinRelayPumpe, relayON);
    DEBUG_print("hotwaterAction(): pump relay: on\n");
  } else if (state == 0) { //&& digitalRead(pinRelayPumpe) == relayON) {
    digitalWrite(pinRelayPumpe, relayOFF);
    DEBUG_print("hotwaterAction(): pump relay: off\n");
  }
}

void steamingAction(int state) {
  steaming = state; //disabled because not yet ready/tested
}

void cleaningAction(int state) { 
  int cleaning = state; 
}
