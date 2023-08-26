#include "rancilio-helper.h"

int signnum(float x) {
  if (x >= 0.0)
    return 1;
  else
    return -1;
}

// returns heater utilization in percent
float convertOutputToUtilisation(double Output, unsigned int windowSize) { return (100 * Output) / windowSize; }

// returns heater utilization in Output
double convertUtilisationToOutput(float utilization, unsigned int windowSize) { return (utilization / 100) * windowSize; }
