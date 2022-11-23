/* This is a sample usage program for the CO2-Backend-Connector.
 * Replace this .ino file with your own project program.
 * start by referencing the library, which should be in the same 
 * folder as your .ino program.
  */
#include "CO2_BackendConnector.h"


bool d1_Ready=false;

CO2_BackendConnector bkend;

void setup() {
  Serial.begin(115200);
  d1_Ready=bkend.d1_ConfigureDevice();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (d1_Ready && (bkend.d1_isConfigured()) && (!bkend.busy)) {
    // random numbers being used for testing purposes in this demonstration program
    bkend.sensor_Temperature = random(20);
    bkend.sensor_VOC = random(100);
    bkend.sensor_CO2 = random(100);
    bkend.d1_PostData("VOC", String(bkend.sensor_VOC));
    bkend.d1_PostData("CELCIUS", String(bkend.sensor_Temperature) );
    bkend.d1_PostData("CO2", String(bkend.sensor_CO2));
    delay(bkend.time_PollInterval);
  }
}
