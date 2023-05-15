#ifndef rancilio_enums_h
#define rancilio_enums_h

#pragma once

// : 0 := OK, 1 := Hardware issue, 2:= Software issue / outlier detected, 3: temperature jump
enum class SensorStatus {
    Ok = 0,
    HardwareIssue = 1,
    SoftwareIssue = 2,
    TemperatureJump = 3
};


enum class State {
  Undefined = 0,
  ColdStart = 1,
  StabilizeTemperature = 2,
  InnerZoneDetected = 3,  // == default
  BrewDetected = 4,
  OuterZoneDetected = 5,
  SteamMode = 6,
  SleepMode = 7,
  CleanMode = 8,
  SoftwareUpdate = 9
};

#endif