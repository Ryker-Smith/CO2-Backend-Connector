#ifndef CO2_BackendConnector_h
#define CO2_BackendConnector_h

#define ver "008"

#include "CO2_BackendCert.h"
#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h> 
#include <ESP8266HTTPClient.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>

#define config_StatusAddress 0 

#define status_OutOfBox   0
#define status_Attempting 4
#define status_Configured 8

#define max_ConfigAttempts 7
#define max_ErrorsAllowed  7

#define max_SSID 32
#define max_PSK 64
#define max_DeviceName 32
#define max_AttemptsReset 10
#define default_config_WiFi_AP_Pass "1234567890"
#define web_BackEnd_HOSTNAME "fachtnaroe.net"
#define web_BackEndServer "https://fachtnaroe.net"
#define web_BackEndScript "/qndco2"
#define web_BackEndPort 443

struct eepromStruct {
    char active;
    byte config_Status;
    byte config_Attempts;
    char config_SSID[max_SSID] = "----------------";
    char config_PSK[max_PSK] = "----------------";
    char config_DeviceName[max_DeviceName] = "";
};

class CO2_BackendConnector {

  public:
    CO2_BackendConnector() { // this is the constructor
      d1_Name=d1_MakeName();
    }
    bool d1_ConfigureDevice();
    bool d1_PostData(String SensorName, String SensorValue);
    void eeprom_Update();
    void eeprom_Debug();
    void reboot();
    bool busy;
    bool d1_isConfigured();
    String d1_MakeName();
    String d1_Name="";
    String rebootstring();
    bool webServerEstablished=false;
    float sensor_Temperature=0.0;
    int   sensor_VOC=0;
    int   sensor_CO2=0;
    const long time_HTTPtransmitTime=1000; // delay thius long after each HTTP POST
    const long time_PollInterval=60000; // delay this long before polling sensors
    bool factoryResetCondition();  // can be use to test for button combinations etc
    
  private:
    eepromStruct eeprom_Data;
    char* default_config_WiFi_AP_SSID="TCFE-CO2";
    byte config_Status;
    byte config_Attempts; 
    void eeprom_Zero();
    void eeprom_NewConfig(String ssid, String psk);
    void eeprom_Special();
    bool eeprom_WebAdminRewrite(eepromStruct newConfig);
    String myMacEnd();
    String getValue(String data, char separator, int index);
    long hstol(String recv);
    
    void establish_AP(String APid, String APpw);
    void establish_WebServerOnAP();
    bool establish_ConnectionToWiFi(String APid, String APpw);
    bool establish_WebServerAsSTA();
};

#endif
