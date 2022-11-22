#include "CO2_BackendConnector.h"
#include "CO2_BackendCert.h"

WiFiClientSecure wifiSecure;
HttpClient http_Client = HttpClient(wifiSecure, web_BackEnd_HOSTNAME, 443);
AsyncWebServer server(80);
IPAddress default_IP(192, 168, 7, 1);
IPAddress default_Gateway(0, 0, 0, 0);
IPAddress default_Subnet(255, 255, 255, 0);

bool CO2_BackendConnector::factoryResetCondition() {
  // this function can check for any GPIO activity designed to enable factory reset
  /*
   * NB NB NB Change to true, upload, then immediately 
   * change to false then re-upload
   */
  return false;
}

bool CO2_BackendConnector::d1_ConfigureDevice() {
  bool result = false;
  busy = true;
  Serial.println("Configuration starts");
  EEPROM.begin(512);
  EEPROM.get(config_StatusAddress, eeprom_Data);
  if (factoryResetCondition()) {
    // do we need to do a factory reset?
    eeprom_Zero();
    reboot();
  }
  eeprom_Debug();
  // for use during development:
//   eeprom_Zero();//
  //eeprom_Special();
  switch (eeprom_Data.config_Status) {
    case  status_Configured : {
        // there's a config which has been used to access the Internet previously
        d1_Name = default_config_WiFi_AP_SSID + myMacEnd();
        // for use during development:
        // eeprom_Special();
        if (!establish_ConnectionToWiFi(eeprom_Data.config_SSID, eeprom_Data.config_PSK)) {
          // if we get here there's a problem with the previously OK credentials
          eeprom_Data.config_Attempts--;
          eeprom_Update();
          if (eeprom_Data.config_Attempts <= 0) {
            // we've tried too often to use the config, reset to defaults
            eeprom_Data.config_Status = status_OutOfBox;
            eeprom_Update();
            reboot();
          }
        }
        else {
          // it works...  Yaa
          Serial.println("Device is configured successfully");
          eeprom_Data.config_Attempts++;
          if (eeprom_Data.config_Attempts > max_ConfigAttempts) {
            eeprom_Data.config_Attempts = max_ConfigAttempts;
            eeprom_Update();
          }
          webServerEstablished = establish_WebServerAsSTA();
          // it works...  aay ... unless
          if (!webServerEstablished) {
            //  ... it doesn't
            // (just in case any problem establishing web data server ... reboot)
            reboot();
          }
          result = true;
        }
      }
      break;
    case  status_Attempting : {
        Serial.println("Device is attempting configuration");
        eeprom_Data.config_Attempts--;
        eeprom_Update();
        // the test for > max_ConfigAttempts is to allow for round-around 0 errors
        if ((eeprom_Data.config_Attempts <= 0) || (eeprom_Data.config_Attempts > max_ConfigAttempts )) {
          // we've tried too often to configure, reset to defaults
          eeprom_Data.config_Attempts = max_ConfigAttempts;
          eeprom_Data.config_Status = status_OutOfBox;
          eeprom_Data.active = 'N';
          eeprom_Update();
          reboot();
        }
        else {
          if (establish_ConnectionToWiFi(eeprom_Data.config_SSID, eeprom_Data.config_PSK)) {
            // we've connected to the WiFI but can we reach the back-end?
            busy = false;
            bool outcome = d1_PostData("IPv4", WiFi.localIP().toString());
            if (outcome) {
              // yes, so save that
              eeprom_Data.config_Status = status_Configured;
              eeprom_Data.active = 'Y';
              eeprom_Data.config_Attempts = max_ErrorsAllowed;
            }
            else {
              Serial.println("Could not save data to BackEnd server");
              eeprom_Data.config_Status = status_Attempting;
              eeprom_Data.active = 'A';
            }
            // one way of the other, save and reboot
            eeprom_Update();
            delay(1000);
            reboot();
          }
        }
      }
      break;
    default: ;
    case  status_OutOfBox  : {
        Serial.println("Device is not configured. Establishing AP.");
        eeprom_Zero();
        eeprom_Update();
        establish_AP(d1_Name, default_config_WiFi_AP_Pass);
        establish_WebServerOnAP();
      }
      break;
  }
  Serial.println("End configuration routine.");
  busy = false;
  return result;
}
bool CO2_BackendConnector::d1_isConfigured() {
  if (eeprom_Data.active == 'Y')
    return true;
  else
    return false;
}

bool CO2_BackendConnector::d1_PostData(String SensorName, String SensorValue) {
  // post data to relay server
  bool result = false;
  if ( !busy ) {
    busy = true;
    Serial.print("Sending data to Relay Server ");
    wifiSecure.setFingerprint(expected_fingerprint_SHA1);
    /* Alternate method is CGI
       String data_ToSend = "device=" + String(eeprom_Data.config_DeviceName) + "&sensor=CO2&value=" + String(sensor_CO2);
       String www_DataType="application/x-www-form-urlencoded"; */
//       String("{\"Status\":\"OK\",\"device\":\"")
    String json_ToSend = String("{\"device\":\"")
                         + String(eeprom_Data.config_DeviceName)
                         + "\",\"sensor\":\"" +  SensorName
                         + "\",\"IPv4\":\"" + WiFi.localIP().toString()
                         + "\",\"value\":\"" + String(SensorValue) + "\"}";
    Serial.println(json_ToSend);
    String json_DataType = "application/json";
    String bes = web_BackEndServer;

    if (http_Client.connect("https://t.fachtnaroe.net", web_BackEndPort)) {
      Serial.println("C3");
    };
    http_Client.post(web_BackEndScript, json_DataType, json_ToSend);
    int statusCode = http_Client.responseStatusCode();
    if (statusCode == 200) {
      Serial.println("Got response "+http_Client.responseBody());
    }
    else {
      Serial.println("Error " + String(statusCode));
    }
    delay (time_HTTPtransmitTime);
    busy = false;
    if (statusCode == 200) {
      result = true;
    }
  }
  return result;
}

String CO2_BackendConnector::d1_MakeName() {
  return default_config_WiFi_AP_SSID + myMacEnd();
}

void CO2_BackendConnector::eeprom_Update() {
  EEPROM.put(config_StatusAddress, eeprom_Data);
  EEPROM.commit();
}

void CO2_BackendConnector::eeprom_Zero() {
  // the device is new and has no settings, so zero-out to make sure
  eeprom_Data.active = 'N';
  eeprom_Data.config_Status = status_OutOfBox;
  eeprom_Data.config_Attempts = 0;
  strncpy(eeprom_Data.config_SSID, "no-SSID-yet--", max_SSID);
  strncpy(eeprom_Data.config_PSK, "no-PSK-yet--", max_PSK);
  d1_Name.toCharArray(eeprom_Data.config_DeviceName, d1_Name.length() + 1);
  eeprom_Update();
}

void CO2_BackendConnector::eeprom_NewConfig(String ssid, String psk) {
  // new settings have been provided, start Attempting to connect
  eeprom_Data.active = 'A';
  eeprom_Data.config_Status = status_Attempting;
  eeprom_Data.config_Attempts = max_ConfigAttempts;
  ssid.toCharArray(eeprom_Data.config_SSID, ssid.length() + 1);
  psk.toCharArray(eeprom_Data.config_PSK, psk.length() + 1);
  d1_Name.toCharArray(eeprom_Data.config_DeviceName, d1_Name.length() + 1);
  eeprom_Update();
}

void CO2_BackendConnector::eeprom_Special() {
  // for development use
  eeprom_Data.active = 'A';
  eeprom_Data.config_Status = 4;
  eeprom_Data.config_Attempts = 3; // errors
  //  strncpy(eeprom_Data.config_SSID, "PutAnSSID_HereToPreConfigure", max_SSID);
  //  strncpy(eeprom_Data.config_PSK, "WithPasswordIfRequired", max_PSK);
  d1_Name = d1_MakeName();
  d1_Name.toCharArray(eeprom_Data.config_DeviceName, d1_Name.length() + 1);
  eeprom_Update();
}

bool CO2_BackendConnector::eeprom_WebAdminRewrite(eepromStruct newConfig) {
  // for development use
  eeprom_Data.active = newConfig.active;
  eeprom_Data.config_Status = newConfig.config_Status;
  eeprom_Data.config_Attempts = newConfig.config_Attempts;
  strncpy(eeprom_Data.config_SSID, newConfig.config_SSID, max_SSID);
  strncpy(eeprom_Data.config_PSK, newConfig.config_PSK, max_PSK);
  d1_Name = d1_MakeName();
  d1_Name.toCharArray(eeprom_Data.config_DeviceName, d1_Name.length() + 1);
  eeprom_Update();
  return true;
}

void CO2_BackendConnector::eeprom_Debug() {
  return;
  Serial.println("====EEPROM DATA====");
  Serial.println("Active: " + String(eeprom_Data.active) );
  Serial.println("Status: " + String(eeprom_Data.config_Status) );
  Serial.println("Count:  " + String(eeprom_Data.config_Attempts) );
  Serial.println("SSID:   " + String(eeprom_Data.config_SSID) );
  Serial.println("PSK:    " + String(eeprom_Data.config_PSK) );
  Serial.println("Device: " + String(eeprom_Data.config_DeviceName) );
  Serial.println("===================");
}

String html_AccessPointConfiguration = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { 
      font-size: 2.8rem; 
    }
    p {
      font-size: 2.0rem; 
    }
    .input-labels {
      font-size: 1.1rem;
      vertical-align:middle;
      padding-bottom: 5px;
    }
    .myinputs {
      font-size: 1.5rem;
      vertical-align:top;
      padding-bottom: 1px;
      border-radius: 3px;
      height: auto;
    }
    .center {
      text-align: center;
      border: none;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <form action="/config" method="GET">
  <h2>D&eacute;an socr&uacute; dom!</h2>
  <p>
    <span id="d1_Name_span">
      <label class="input-labels">Your device name is:<br></label> 
      <label type="text" readonly class="myinputs center" id="d1_Name">%d1_Name_placeholder%</label>
    </span>
  <p>
    <span id="ssid_name_span">
      <label class="input-labels">Please provide the Wi-Fi network name:</label> 
      <input type="text" class="myinputs" name="ssid_name" id="ssid_name" required value="">
    </span>
  </p>
  <p>
    <span id="ssid_pw_span">
      <label for="ssid_pw" class="input-labels">Please provide the Wi-Fi password:</label>
      <input type="text" class="myinputs" name="ssid_pw" id="ssid_pw" required value="">
    </span>
  </p>
  <p>
    <input type="submit" class="myinputs" value="Send these to your device">
  </p>
  </form>
</body>
</html>)rawliteral";

String html_ConfigurationReceived = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { 
      font-size: 2.8rem; 
    }
    p {
      font-size: 2.0rem; 
    }
    .input-labels {
      font-size: 1.1rem;
      vertical-align:middle;
      padding-bottom: 5px;
    }
    .myinputs {
      font-size: 1.5rem;
      vertical-align:top;
      padding-bottom: 1px;
      border-radius: 3px;
      height: auto;
    }
    .center {
      text-align: center;
      border: none;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <form >
  <h2>Ceangail liom ar-l&iacute;ne anois!</h2>
  <p>
    <span id="d1_Name_span">
      <label class="input-labels">Your device name is:<br></label> 
      <label type="text" readonly class="myinputs center" id="d1_Name">%d1_Name_placeholder%<br></label>
      <label class="input-labels">You should now connect to your normal network, then use your CO2 monitoring App, and use the device name above to connect to your CO2 sensor device.</label>
    </span>
  </p>
</body>
</html>)rawliteral";

/* Version 003 is necessitated by the missing 'v' in the label IPv4!
   Any other changes are opportunistic
*/
String data_json_1 = R"=====({ "Status" : "%RES%", "CO2" : "%CO2%", "VOC" : "%VOC%", "CELCIUS" : "%TEMPERATURE%", "IPv4" : "%IPv4%" })=====";
String data_json_all = R"=====({ "Status" : "%RES%", "active" : "%ACTIVE%", "config_Status" : "%STATUS%", "config_Attempts" : "%ATTEMPTS%", "config_SSID" : "%SSID%", "config_PSK" : "%PSK%", "config_DeviceName" : "%NAME%" })=====";

long CO2_BackendConnector::hstol(String recv) {
  return strtol(recv.c_str(), NULL, 16);
}

String CO2_BackendConnector::getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

String CO2_BackendConnector::myMacEnd() {
  String MAC = WiFi.macAddress();
  String str = String();
  int n = 0;
  String _myAPid = "";
  while (n < 6) {
    str = getValue(MAC, ':', n);
    if (n >= 4) {
      _myAPid = _myAPid + "-" + str;
    }
    n++;
  }
  return _myAPid;
}

String CO2_BackendConnector::rebootstring() {
  String s="/";
  s+="athbhútáil";
//  s+=myMacEnd();
  return s;
}

void CO2_BackendConnector::reboot() {
  Serial.println("Calling reboot() now.");
  ESP.reset();
}

void CO2_BackendConnector::establish_AP(String APid, String APpw) {
  Serial.println("Access Point starting...");
  Serial.println("Establishing AP with SSID " + APid + " and password " + APpw);
  String str_default_IP = default_IP.toString();
  Serial.println("Please connect to AP, then use browser to view and configure at " + str_default_IP);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(default_IP, default_Gateway, default_Subnet);
  WiFi.softAP(APid, APpw);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IPv4 address: ");
  Serial.println(IP);
  Serial.println("Access point started.");
}

void CO2_BackendConnector::establish_WebServerOnAP() {
  Serial.println("Web Server for AP starting");
  // Route for root / web page used for initial configuration
  server.on("/", HTTP_GET, [ = ](AsyncWebServerRequest * request) {
    html_AccessPointConfiguration.replace("%d1_Name_placeholder%", this->d1_Name);
    request->send(200, "text/html", html_AccessPointConfiguration);
    Serial.println("First page sent");
  });
  Serial.println("Q1");
  server.on("/config", HTTP_GET, [ = ](AsyncWebServerRequest * request) {
    Serial.println("Second page sent");
    String wifi_SSID = "gotsomething";
    String wifi_Pass;
    int paramsNr = request->params();
    for (int i = 0; i < paramsNr; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->name() == "ssid_name") {
        wifi_SSID = p->value();
      }
      if (p->name() == "ssid_pw") {
        wifi_Pass = p->value();
      }
    }
    if (wifi_SSID != "gotsomething") {
      html_ConfigurationReceived.replace("%d1_Name_placeholder%", this->d1_Name);
      request->send(200, "text/html", html_ConfigurationReceived);
      server.end();
      Serial.println("AP Webserver server ended; attempting configuration");
      eeprom_NewConfig(wifi_SSID, wifi_Pass);
      delay(5000);
      this->reboot();
    }
    else {
      Serial.println("Problem with configuration or data");
    }
  });
  Serial.println("Q2");
  // Start server
  server.begin();
  Serial.println("Web-server started on AP");
}

bool CO2_BackendConnector::establish_ConnectionToWiFi(String APid, String APpw) {
  Serial.println("Connecting to Wi-Fi");
  /* this next part was trialled as an alternative to the device
     resetting itself . The solution to changing from AP mode to
     STA mode was unclear, believed to be a result of re-suing the
     wifi config record ... or such
  */
  station_config* mt;
  if (!WiFi.softAPdisconnect()) {
    mt = {0};
    wifi_station_set_config(mt);
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(APid, APpw);
  WiFi.hostname(eeprom_Data.config_DeviceName);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    counter++;
    if (counter >= max_AttemptsReset) {
      Serial.println(eeprom_Data.config_Attempts);
      eeprom_Data.config_Attempts -= 1;
      Serial.println(eeprom_Data.config_Attempts);
      eeprom_Update();
      delay(500);
      reboot();
    }
  }
  Serial.println();
  Serial.print("WiFI connected, IP address: ");
  Serial.println(WiFi.localIP());
  return true;
}

/* This code must be outside of the function in which it is used, as inside they would be local
  in scope, but the callback function will not be 'local' when executed asynchronously
  Inside the chevrons, 200 is the capacity of the memory pool in bytes. */
StaticJsonDocument<256> json_Decoded;
const char* PARAM_MESSAGE_BODY = "body";

bool CO2_BackendConnector::establish_WebServerAsSTA() {
  Serial.println("Web server started for data");
  /* Route for root / for anything received on web after configuration
    response is the current data, stored in:
    sensor_CO2
    sensor_VOC
    sensor_Temperature
  */
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest * request) {
    Serial.println("->GET /");
    data_json_1.replace("%RES%", "OK");
    data_json_1.replace("%CO2%", String(sensor_CO2));
    data_json_1.replace("%VOC%", String(sensor_VOC));
    data_json_1.replace("%TEMPERATURE%", String(sensor_Temperature));
    data_json_1.replace("%IPv4%", WiFi.localIP().toString());
    request->send(200, "text/html", data_json_1 );
    Serial.println("  reply out = " + data_json_1);
  });

  server.on("/getconfig", HTTP_GET, [this](AsyncWebServerRequest * request) {
    Serial.println("->GET /getconfig");
    data_json_all.replace("%RES%", "OK");
    data_json_all.replace("%ACTIVE%", String(eeprom_Data.active));
    data_json_all.replace("%STATUS%", String(eeprom_Data.config_Status));
    data_json_all.replace("%ATTEMPTS%", String(eeprom_Data.config_Attempts));
    data_json_all.replace("%SSID%", String(eeprom_Data.config_SSID));
    data_json_all.replace("%PSK%", String(eeprom_Data.config_PSK));
    data_json_all.replace("%NAME%", String(eeprom_Data.config_DeviceName));
    request->send(200, "text/html", data_json_all );
    Serial.println("  reply out = " + data_json_all);
  });

  char str[20];
  String s;
  s=rebootstring();
  s.toCharArray(str,20);
  
  server.on(str, HTTP_GET, [this](AsyncWebServerRequest * request) {
    request->send(200, "application/json");
//AwsResponseFiller
//    request->send("application/json", 20,  {delay(5000)});
    Serial.println("->GET /reboot");
    server._handleDisconnect(request);
    server.end();
//    delay(1000);
    this->reboot();
  });
  
  server.on("/setconfig", HTTP_POST, [this](AsyncWebServerRequest * request) {
    char postdata_Raw[256];
    Serial.println("->POST /setconfig");
    if (request->hasParam(PARAM_MESSAGE_BODY, true)) {
      request->getParam(PARAM_MESSAGE_BODY, true)->value().toCharArray(postdata_Raw, 256);
    }
    
    // now process
    DeserializationError json_Error = deserializeJson(json_Decoded, postdata_Raw);
    if (!json_Error) {
      // if we get here then the in-bound data is formatted OK, so now extract to my variables
      eepromStruct newConfig = {};
      newConfig.config_Status=json_Decoded["config_Status"];
      newConfig.config_Attempts=json_Decoded["config_Attempts"];
      strncpy(newConfig.config_SSID, json_Decoded["config_SSID"], max_SSID);
      strncpy(newConfig.config_PSK, json_Decoded["config_PSK"], max_PSK);
      // protocol to support changing the DeviceName jnot yet clear
      // so just re-use the default (calculated) one. This is redundant as it is repeated in the 
      // eeprom rewrite function called 
      d1_Name.toCharArray(newConfig.config_DeviceName, d1_Name.length() + 1);
      const char* tmp_active=json_Decoded["active"];
      char oneChar=tmp_active[0];
      newConfig.active=oneChar;
      if (eeprom_WebAdminRewrite(newConfig)) {
        Serial.println("  reply out = 200, OK");
        // not sure anything beyond a 200 is required; let the app re-read to check
        request->send(200, "application/json");
      }
    }
    else {
      Serial.println("Error deserializing JSON data");
    }
  });

  server.onNotFound ( [this](AsyncWebServerRequest * request) {
    request->send(404, "text/plain", "Not Found");
  });

  Serial.println("W1");
  // Start server
  server.begin();
  Serial.println("Web-server established");
  webServerEstablished = true;
  return true;
}

// here be monsters:
//    for (int i=0; i < request->headers(); i++) {
//      Serial.print(request->headerName(i));
//      Serial.print(" ===> ");
//      Serial.println(request->header(i));
//    }
//    for (int i=0; i < request->args(); i++) {
//      Serial.print(request->argName(i));
//      Serial.print(" ===> ");
//      Serial.println(request->arg(i));
//    }
