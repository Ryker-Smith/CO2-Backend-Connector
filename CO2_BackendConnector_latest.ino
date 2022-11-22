
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
    bkend.sensor_Temperature = random(20);
    bkend.sensor_VOC = random(100);
    bkend.sensor_CO2 = random(100);
    bkend.d1_PostData("VOC", String(bkend.sensor_VOC));
    bkend.d1_PostData("CELCIUS", String(bkend.sensor_Temperature) );
    bkend.d1_PostData("CO2", String(bkend.sensor_CO2));
    delay(bkend.time_PollInterval);
  }
}
