#include "ZACwire.h"

ZACwire<17> Sensor(306,140,10,1);		// set pin "2" to receive signal from the TSic "206" with an expected bitWindow of "140µs" and an offset of "10µs". ISR executed on CPU1

float Input = 0;
unsigned long previousMillistemp;       // initialisation at the end of init()
const long refreshTempInterval = 100;  //How often to read the temperature sensor
unsigned long currentMillistemp = 0;

char debugline[120];
unsigned long loops = 0;
unsigned long max_micros = 0;
unsigned long last_report_micros = 0;
unsigned long cur_micros_previous_loop = 0;
const unsigned long loop_report_count = 100;
unsigned long cur_micros = 0;


void setup() {
  Serial.begin(115200);
  
  if (Sensor.begin() == true) {     //check if a sensor is connected to the pin
    Serial.println("Signal found on pin 2");
  }
  else Serial.println("No Signal");
  delay(120);
}

void loop() {

  loops += 1 ;
  cur_micros = micros();
  if (max_micros < cur_micros-cur_micros_previous_loop) {
      max_micros = cur_micros-cur_micros_previous_loop;
  }

  if ( cur_micros >= last_report_micros + 100000 ) { //100ms
    snprintf(debugline, sizeof(debugline), "%lu loop() temp=%0.2f | loops/ms=%lu | spend_micros_last_loop=%lu | max_micros_since_last_report=%lu | avg_micros/loop=%lu\n", 
        cur_micros/1000, Input, loops/100, (cur_micros-cur_micros_previous_loop), max_micros, (cur_micros - last_report_micros)/loops );
    Serial.print(debugline);
    //Serial.println(max_micros);
    //Serial.println(loops/10);
    last_report_micros = cur_micros;
    max_micros = 0;
    loops=0;
    
  }
  cur_micros_previous_loop = cur_micros; // micros();
  

  currentMillistemp = millis();
  if (currentMillistemp >= previousMillistemp + refreshTempInterval) {
      previousMillistemp = currentMillistemp;

      Input = Sensor.getTemp();     //get the Temperature in °C 
      if (Input == 222) {
        Serial.println("Reading failed");
      } 
      /*
      else {
        //Serial.print("Temp: ");
        Serial.println(Input);
      }
      #*/
    }

}
