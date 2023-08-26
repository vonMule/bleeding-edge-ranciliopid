#ifndef HELPER_H
#define HELPER_H

int signnum(float x);

// returns heater utilization in percent
float convertOutputToUtilisation(double, unsigned int);
// returns heater utilization in Output
double convertUtilisationToOutput(float, unsigned int);

#endif // HELPER_H