//#include <ESP8266WiFi.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ezTime.h>
#include <Preferences.h>
#include <Update.h>
#include <SHTSensor.h>

SHTSensor sht(SHTSensor::SHT3X);

//hw_timer_t * timer = NULL;

//portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define buttonReset 2 // Pin boot
#define buttonConfirm 0
#define buttonClickTime 300 // Button click time, @exp
#define defaultAPPassword "12345678"
#define defaultAPSsid  "ESP32"
#define defaultWiFiSsid  ""
#define defaultWiFiPassword  ""
#define defaultlogDataUrl  "https://google.com/logdata.php"
#define defaultuserId  "0"
#define defaultdevicePassword "admin"
#define defaulttimezoneLocation "Europe/Samara"

//config for AP
String APSsid = defaultAPSsid;
String APPassword = defaultAPPassword;

unsigned long APStarted = 0;
int APModeDurationAfterConnectionFailed = 60000;

IPAddress ip(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 254);
IPAddress subnet(255, 255, 255, 0);

//config for WiFi connection
String WiFiSsid = defaultWiFiSsid;
String WiFiPassword = defaultWiFiPassword;
bool isLastConnectionFailed = false;


//config for dataLoger
String logDataUrl = defaultlogDataUrl;
String userId = defaultuserId;
#define mesurmentDealy 1000
#define mesurmentStack 10000
#define mesurmentsCount 10

//time
String timezoneLocation = defaulttimezoneLocation;

//?
String devicePassword = defaultdevicePassword;


int currentRunLevel = 0;

//0 AP need setup
//1 AP runing
//2 trying to connect to wifi
//3 conected to wifi sending data
//99 update

// bool mainMode = false;
// bool mainModeHasInvoked = false;
// bool updating = false;

WebServer server(80);

//config Save
Preferences preferences;


float randomFloat(float a, float b) {
  float random = ((float) rand()) / (float) RAND_MAX;
  float diff = b - a;
  float r = random * diff;
  return a + r;
}



class SHT30Data {
private:
  float humidity[mesurmentsCount];
  float temerature[mesurmentsCount];
  unsigned long lastUpdate = 0;
  unsigned long lastDataSend = 0;
  int latNotEmptyHumidity = 0;
  int latNotEmptyTemerature = 0;
  void shiftData(float arr[], int size) {
    for (int i = 0; i< size-1; i++) {
      arr[i] = arr[i+1];
    }
  }
  float avgFromArray(float* array, int size) {
    float sum = 0;
    for (int i = 0; i < size; i++) {
      sum += array[i];
    }
    return (sum/size);
  }

  String getStringFromArray(float* array, int size) {
    String out = "";
    for (int i = 0; i < size; i++) {
      if (i == 0) {
        out = String(array[i]);
      }
      else {
          out += ", " + String(array[i]);
      }
    }
    return out;
  }
public:

  void pushNewHumidity(float humidity) {
    if (latNotEmptyHumidity < mesurmentsCount) {
      this->humidity[latNotEmptyHumidity] = humidity;
      latNotEmptyHumidity++;
    } else {
      shiftData(this->humidity,mesurmentsCount);
      this->humidity[mesurmentsCount-1] = humidity;
    }
  }
  void pushNewTemerature(float temerature) {
    if (latNotEmptyTemerature < mesurmentsCount) {
      this->temerature[latNotEmptyTemerature] = temerature;
      latNotEmptyTemerature++;
    } else {
      shiftData(this->temerature,mesurmentsCount);
      this->temerature[mesurmentsCount-1] = temerature;
    }
  }
  float getAverageHumidity() {
    return avgFromArray(humidity,mesurmentsCount);
  }
  float getAverageTemerature() {
    return avgFromArray(temerature,mesurmentsCount);
  }
  void updateCurretValues() {
    if (millis() >= lastUpdate+mesurmentDealy) {
      if (sht.readSample()) {
        pushNewHumidity(sht.getHumidity());
        pushNewTemerature(sht.getTemperature());
      } else {
        Serial.print("[SHT3X] Error in readSample()\n");
      }

      //pushNewHumidity(randomFloat(0.0,99.9));
      //pushNewTemerature(randomFloat(-40.0,80.0));
      lastUpdate = millis();
    }
  }
  bool isNeedToSend() {
    if (millis() >= lastDataSend+mesurmentStack) {
      return true;
    }
    return false;
  }
  void setlastDataSend(unsigned long millis) {
    lastDataSend = millis;
  }

  String getStringHumidity() {
    return getStringFromArray(humidity, mesurmentsCount);
  }
  String getStringTemerature() {
    return getStringFromArray(temerature, mesurmentsCount);
  }
};


SHT30Data SHT30Data;

class Button {
private:
  int pinNumber;
  bool pressed = false;
  bool down = false;
  unsigned long rearMillis = 0;
  unsigned long frontMillis = 0;
  bool clicked = false;
  int clicksCount = 0;
  int clicks = 0;
  unsigned long forgetClicksTime = buttonClickTime * 1.5 ;
public:
  Button(int pin) {
    pinNumber = pin;
  }

  bool getDownState() {
    return down;
  }
  bool getPressedState() {
    return pressed;
  }
  unsigned long getLastPressed() {
    return millis() - rearMillis;
  }
  unsigned long getPressTime() {
    return millis() - frontMillis;
  }
  unsigned long getPressedTime() {
    return rearMillis - frontMillis;
  }

  void readState() {
    if (digitalRead(pinNumber)==LOW) {
      if (pressed == false) {
        pressed = true;
        down = true;
        frontMillis = millis();
        rearMillis = millis();
        clicksCount++;
        Serial.println("clicksCount" + String(clicksCount));
      }
    }
    else if (digitalRead(pinNumber)==HIGH) {
      if ((pressed == true) && (getPressTime() >= 10)) {
        pressed = false;
        down = false;
        rearMillis = millis();
        if (getPressTime() <= buttonClickTime) {
          clicked = true;
        }
      }
      if (millis()-rearMillis>=forgetClicksTime && clicksCount > 0) {
        clicks = clicksCount;
        clicksCount = 0;
      }
    }
  }
  bool buttonClicked () {
    if (clicked) {
      clicked = false;
      return true;
    }
    else return false;
  }
  int getClicks() {
    if (clicks > 0) {
      int outR = clicks;
      clicks = 0;
      return outR;
    }
    return 0;
  }

};

Button bReset(buttonReset);
Button bConfirm(buttonConfirm);


bool loadConfig() {
  bool out = false;
  Serial.println("Trying to load config.");
  preferences.begin("config", false);
  if (preferences.getString("wifi_ssid","").length() > 0 &&  preferences.getString("wifi_password","").length() > 0) {
    Serial.println("===WiFi config===");
    WiFiSsid = preferences.getString("wifi_ssid");
    WiFiPassword = preferences.getString("wifi_password");
    Serial.println(("wifi_ssid = " + WiFiSsid));
    Serial.println(("wifi_password = " + WiFiPassword));
    out = true;
  }
  if (preferences.getString("log_data_url","").length() > 0) {
    Serial.println("===Data send config===");
    logDataUrl = preferences.getString("log_data_url");
    Serial.println(("log_data_url = " + logDataUrl));
  }
  if (preferences.getString("user_id","").length() > 0) {
    userId = preferences.getString("user_id");
    Serial.println(("user_id = " + userId));
  }
  if (preferences.getString("ap_ssid","").length() > 0 &&  preferences.getString("ap_assword","").length() > 0) {
    Serial.println("===AP config===");
    APSsid = preferences.getString("ap_ssid");
    APPassword = preferences.getString("ap_assword");
    Serial.println(("ap_ssid = " + APSsid));
    Serial.println(("ap_assword = " + APPassword));
  }
  if (preferences.getString("dev_password","").length() > 0) {
    Serial.println("===Device password===");
    devicePassword = preferences.getString("dev_password");
    Serial.println("dev_password (ONLY LENGHT) = " + String(devicePassword.length()));
  }
  if (preferences.getString("timezone","").length() > 0) {
    Serial.println("===Device time===");
    timezoneLocation = preferences.getString("timezone");
    Serial.println("timezone = " + timezoneLocation);
  }

  preferences.end();
  return out;
}

void saveConfigToPref() {
  Serial.println("Trying to save config.");
  preferences.begin("config", false);

  if (WiFiSsid != defaultWiFiSsid && WiFiPassword != defaultWiFiPassword) {
    preferences.putString("wifi_ssid",WiFiSsid);
    preferences.putString("wifi_password",WiFiPassword);
  }
  if (logDataUrl != defaultlogDataUrl) {
    preferences.putString("log_data_url",logDataUrl);
  }
  if  (APSsid != defaultAPSsid  && APPassword != defaultAPPassword ) {
    preferences.putString("ap_ssid",APSsid);
    preferences.putString("ap_assword",APPassword);
  }
  if (userId != defaultuserId) {
    preferences.putString("user_id",userId);
  }

  if (devicePassword != defaultdevicePassword) {
    preferences.putString("dev_password",devicePassword);
  }

  preferences.putString("timezone",timezoneLocation);


  preferences.end();
}

void resetConfig() {
  Serial.println("===Reseting configs===");
  preferences.begin("config", false);
  preferences.clear();
  preferences.end();
}

String sendGetRequest(String url) {
  WiFiClient client;
  HTTPClient http;
  String out = "";
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      const String& payload = http.getString();
      out = payload;
    }
  }
  else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  return out;
}

String sentData(String logData, String url) {
  WiFiClient client;
  HTTPClient http;
  String out = "";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  Serial.println("[HTTP] POST:");
  Serial.println(logData);
  int httpCode = http.POST(logData);

  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      const String& payload = http.getString();
      Serial.println("received payload:\n<<");
      Serial.println(payload);
      out = payload;
      Serial.println(">>");
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  return out;
}

String generatefullHtml(String tamplate) {
  String start = "<html><head><meta charset='utf-8'><title>ESP32</title><meta name='viewport' content='width=device-width,initial-scale=1'><style type='text/css'>*,::after,::before{box-sizing:border-box;font-family:Gotham Rounded,sans-serif;font-weight:400}body{padding:0;margin:0;background:linear-gradient(to right,#0AF,#00FF6C)}.d-flex{display:flex;justify-content:center;align-items:center}.ssid-set{padding:20px 0}.ssid-set__form{max-width:640px;line-height:26px;position:relative;text-align:center}.logger-current-data{padding:20px 0}.app{margin-top:100px}.wrapper{max-width:640px;margin:auto;background:#fff;padding:40px 20px}.fwupdate{display:flex;justify-content:center;align-items:center}.config-form{padding:20px 0}.config-form>div>div{padding:5px 0}.plate{width:160px;height:160px;display:flex;position:relative;align-items:center;justify-content:center;color:#ecf0f1;margin:10px;padding:10px;background:#7f8c8d}.plate__title{bottom:10px;position:absolute;font-size:14px}.plate__data{font-size:50px}.plate_temperature{background:#2980b9}.plate_humidity{background:#16a085}.plate_temperature>.plate__data::after{content:'\\2103';font-size:15px}.plate_humidity>.plate__data::after{content:'%';font-size:15px}.advance-data{font-size:15px}.input-container>label{display:block;font-size:12px}.input-container>input{padding:5px 0;border:none;border-bottom:2px solid #ecf0f1;width:100%;font-size:16px}.input-container>input:focus{outline:0;border:none;border-bottom:2px solid #3498db}.config-item{padding-bottom:15px}.config-item>div:nth-child(1){font-size:12px}.config-item>div:nth-child(2){padding-top:5px;font-size:16px;word-wrap:break-word}</style></head><body></body></html>";
  String end = "</body></html>";
  return start+tamplate+end;
}

void handleNotFound(){
  server.send(404, "text/plain", "404: Not found");
}

void handleRoot() {
  String outLoggerData = "<div class='logger-current-data'><div class='plates d-flex'><div class='plate plate_temperature'><div class='plate__title'>temperature AVG</div><div class='plate__data'>"+String(SHT30Data.getAverageTemerature())+"</div></div><div class='plate plate_humidity'><div class='plate__title'>humidity AVG</div><div class='plate__data'>"+String(SHT30Data.getAverageHumidity())+"</div></div></div><div class='advance-data d-flex'><div class='plate'><div class='plate__title'>temperature</div>" + SHT30Data.getStringTemerature() + "</div><div class='plate'><div class='plate__title'>humidity</div>" + SHT30Data.getStringHumidity() + "</div></div></div>";
  if (currentRunLevel == 1) {
    String wifiConfigForm = "<form class='config-form' action='/setupwifi' method='POST'><h3>Please input ssid and password</h3><div class='input-container'><label for='wifi_ssid'>Wifi SSID:</label><input id='wifi_ssid' name='ssid'></div><div class='input-container'><label for='wifi_password'>Wifi password:</label><input id='wifi_password' name='password'></div><input type='submit' value='Save settings'></form>";

    String outhtml = "<div class='app'><div class='wrapper'>" + wifiConfigForm + outLoggerData + "</div></div>";
    server.send(200, "text/html", generatefullHtml(outhtml));
  }
  else if (currentRunLevel == 3) {
    String outConfigData = "<div class='logger-config'><h3>Current configurations</h3><div class='config-item'><div>AP ssid:</div><div>" + APSsid + "</div></div><div class='config-item'><div>AP password:</div><div>" + APPassword + "</div></div><div class='config-item'><div>WIFI ssid:</div><div>" + WiFiSsid + "</div></div><div class='config-item'><div>WIFI password:</div><div>" + WiFiPassword + "</div></div><div class='config-item'><div>URL for data logging:</div><div>" + logDataUrl + "</div></div><div class='config-item'><div>User id:</div><div>" + userId + "</div></div><div class='config-item'><div>Time zone:</div><div>" + timezoneLocation + "</div></div></div>";

    String outConfigForm = "<form class='config-form' action='/set' method='post'><div><h3>Configurations for Access Point mode</h3><div class='input-container'><label for='new_ap_ssid'>New AP SSID:</label><input id='new_ap_ssid' name='new_ap_ssid'></div><div class='input-container'><label for='new_ap_password'>New AP password:</label><input id='new_ap_password' name='new_ap_password'></div></div><div><h3>Configurations for WiFi station mode</h3><div class='input-container'><label for='new_wifi_ssid'>New WiFi SSID:</label><input id='new_wifi_ssid' name='new_wifi_ssid'></div><div class='input-container'><label for='new_wifi_password'>New WiFi password:</label><input id='new_wifi_password' name='new_wifi_password'></div></div><div><h3>Configurations for data logging</h3><div class='input-container'><label for='new_logdata_url'>New URL for data logging:</label><input id='new_logdata_url' name='new_logdata_url'></div><div class='input-container'><label for='new_user_id'>New user id for data logging:</label><input id='new_user_id' name='new_user_id'></div><div class='input-container'><label for='new_timezone'>New timezone data logging:</label><input id='new_timezone' name='new_timezone'></div></div><div><h3>Configurations for device security</h3><div class='input-container'><label for='new_device_password'>New device password:</label><input id='new_device_password' name='new_device_password'></div></div><br><div><label for='device_password'>Current password to confirm changes:</label><input required id='device_password' name='device_password'></div><br><input type='submit' value='Save settings'></form>";
    String outhtml = "<div class='app'> <div class='wrapper'><div class='logger-main'>" +  outLoggerData + outConfigData + outConfigForm +"</div></div></div>";
    server.send(200, "text/html", generatefullHtml(outhtml));

  }
}

void handleSetFWUpdate() {
  if((server.hasArg("isfwupdatestate")) && (server.arg("isfwupdatestate") != NULL)) {
    if (server.arg("isfwupdatestate") == "true") {
      currentRunLevel = 99;
      server.sendHeader("Location", "/fwupdate",true);
    }
    else {
      currentRunLevel = 0;
      server.sendHeader("Location", "/",true);
    }
  }

  server.send(302, "text/plane","");
}

void handleFWUpadePage() {
  String outForm = "";
  if (currentRunLevel != 99) {
    outForm = "<div class='app'><div class='wrapper'><form class='fwupdate d-flex' method='POST' action='/setupdate'><input type='hidden' name='isfwupdatestate' value='true'><input type='submit' value='Set update state!'></form></div></div>";
  }
  else if (currentRunLevel == 99) {
    outForm = "<div class='app'> <div class='wrapper'><form class='fwupdate d-flex' method='POST' action='/setupdate'><input type='hidden' name='isfwupdatestate' value='false'><input type='submit' value='Cancel update state'></form><form class='fwupdate d-flex' method='POST' action='/fwupdate' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form></div></div>";
  }
  server.send(200, "text/html", generatefullHtml(outForm));
}

void handleSetWifiConfig() {
  if (currentRunLevel == 1) {
    if ((server.hasArg("ssid") && server.hasArg("password")) && ((server.arg("ssid") != NULL || server.arg("password") != NULL))) {
      if (String(server.arg("ssid")).length() > 0 && String(server.arg("password")).length() >= 8) {
        WiFiSsid = String(server.arg("ssid"));
        WiFiPassword = String(server.arg("password"));
        currentRunLevel = 2;
        saveConfigToPref();
        server.sendHeader("Location", "/",true);
        server.send(302, "text/plane","");
        return;
      }
    }
    else {
      server.send(400, "text/plain", "400: Invalid Request");
      return;
    }
  }
  handleNotFound();
}

void handleSetConfig() {
  if (currentRunLevel !=99) {
    if((server.hasArg("device_password") && server.arg("device_password") != NULL && String(server.arg("device_password")) == devicePassword)) {

      if((server.hasArg("new_device_password") && server.arg("new_device_password") != NULL && String(server.arg("new_device_password")).length() > 0)) {
        devicePassword = String(server.arg("new_device_password"));
      }

      if((server.hasArg("new_ap_ssid") && server.arg("new_ap_ssid") != NULL && String(server.arg("new_ap_ssid")).length() > 0)) {
        if((server.hasArg("new_ap_password") && server.arg("new_ap_password") != NULL && String(server.arg("new_ap_password")).length() >= 8)) {
          APSsid = String(server.arg("new_ap_ssid"));
          APPassword = String(server.arg("new_ap_password"));
        }

      }

      if((server.hasArg("new_wifi_ssid") && server.arg("new_wifi_ssid") != NULL && String(server.arg("new_wifi_ssid")).length() > 0)) {
        if((server.hasArg("new_wifi_password") && server.arg("new_wifi_password") != NULL && String(server.arg("new_wifi_password")).length() >= 8)) {
          WiFiSsid = String(server.arg("new_wifi_ssid"));
          WiFiPassword = String(server.arg("new_wifi_password"));
          Serial.println(WiFiSsid);
          Serial.println(WiFiPassword);
        }
      }

      if((server.hasArg("new_logdata_url") && server.arg("new_logdata_url") != NULL && String(server.arg("new_logdata_url")).length() > 0)) {
        logDataUrl = String(server.arg("new_logdata_url"));
      }

      if((server.hasArg("new_user_id") && server.arg("new_user_id") != NULL && String(server.arg("new_user_id")).length() > 0)) {
        userId = String(server.arg("new_user_id"));
      }

      if((server.hasArg("new_timezone") && server.arg("new_timezone") != NULL && String(server.arg("new_timezone")).length() > 0)) {
        timezoneLocation = String(server.arg("new_timezone"));
      }
      saveConfigToPref();
      server.sendHeader("Location", "/",true);
      server.send(302, "text/plane","");
      return;
    }
  }
  handleNotFound();
}

void setUpServer() {
  server.onNotFound(handleNotFound);

  server.on("/", HTTP_GET, handleRoot);


  server.on("/set", HTTP_POST, handleSetConfig);

  server.on("/setupwifi", HTTP_POST, handleSetWifiConfig);

  server.on("/setupdate", HTTP_POST, handleSetFWUpdate);
  server.on("/fwupdate", HTTP_GET, handleFWUpadePage);
  server.on("/fwupdate", HTTP_POST, []() {
    server.sendHeader("Location", "/",true);
    server.send(302, "text/plane",(Update.hasError()) ? "FAIL" : "OK");
    currentRunLevel = -1;
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin()) { //start with max available size
        Update.printError(Serial);
      }
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    }
    else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      }
      else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    else {
      Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
    }
  });

  server.begin();

  Serial.println("HTTP server started");
}

void setdownWifi() {
  // server.stop();
  // WiFi.softAPdisconnect();
  // WiFi.persistent(false);
  // WiFi.disconnect(true);
  // WiFi.mode(WIFI_OFF);
}

void setupForWifiAP(String ssid, String password) {
  setdownWifi();

  loadConfig();
  if (!isLastConnectionFailed) {
    if (WiFiPassword.length() > 0 && WiFiPassword.length() > 0) {
      currentRunLevel = 2;
      return;
    }
  }

  WiFi.mode(WIFI_AP);
  Serial.println("Configuring access point...");

  if(WiFi.softAP(ssid.c_str(), password.c_str())){
    Serial.println("Wait 100 ms for AP_START...");
    delay(100);
    // if(!WiFi.softAPConfig(ip, gateway, subnet)){
    //     Serial.println("AP Config Failed");
    // }
    IPAddress myIP = WiFi.softAPIP();
    Serial.println("Network " + String(ssid) + " running");
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    APStarted = millis();
    //setUpServer();
  } else {
    Serial.println("Starting AP failed.");
  }

}



bool setupForWifiConection(String ssid, String password) {
  setdownWifi();


  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  delay(100);

  Serial.println("Waiting to connect...");
  int attempts = 0;
  delay(1000);
  Serial.println(WiFi.status());
  while (WiFi.status() != WL_CONNECTED) {
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("WL_CONNECT_FAILED");
    }
    Serial.println(WiFi.status());
    attempts++;
    delay(250);
    Serial.println(".");
    if (attempts >= 20) {
      Serial.println("No success. Returning to AP mode fo 60 sec!");
      isLastConnectionFailed = true;
      currentRunLevel = 0;
      return false;
    }
  }
  isLastConnectionFailed = false;
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //setUpServer();
  return true;
}



void setup() {
  pinMode(buttonReset, INPUT);
  pinMode(buttonReset, INPUT_PULLUP);
  pinMode(buttonReset, INPUT_PULLUP);

  pinMode(buttonConfirm, INPUT);
  pinMode(buttonConfirm, INPUT_PULLUP);

  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  ledcSetup(20, 4000, 8);
  Wire.begin();
  Serial.begin(115200);
  delay(3000);



  if (sht.init()) {
    Serial.print("[SHT3X] init(): success\n");
  } else {
    Serial.print("[SHT3X] init(): failed\n");
  }
  sht.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM); // only supported by SHT3x

  WiFi.mode(WIFI_AP);
  setUpServer();
}

void loop() {

  //ledcWrite(20, 255);
  bReset.readState();
  bConfirm.readState();
  server.handleClient();



  if (currentRunLevel == 99) {

  }
  else {
    //Serial.println(currentRunLevel);
    SHT30Data.updateCurretValues();

    if (bReset.getDownState() && bReset.getPressTime() >= 5000) {
      if (bConfirm.getDownState() && bConfirm.getPressTime() >= 7000) {
        resetConfig();
      }
      currentRunLevel = -1;
    }
    if (currentRunLevel == 0) {
      Serial.println("\n===Going for runlevel 1===\n");
      currentRunLevel = 1;
      setupForWifiAP(APSsid, APPassword);

    }
    if (currentRunLevel == -1) {
      Serial.println("\n===Going for reboot===\n");
      ESP.restart();
    }
    else if (currentRunLevel == 1) {
      if ((millis() >= APStarted + APModeDurationAfterConnectionFailed) && isLastConnectionFailed) {
        if (WiFiPassword.length() > 0 && WiFiPassword.length() > 0) {
          Serial.println("\n===Going for runlevel 2===\n");
          currentRunLevel = 2;
        }
        Serial.println("Trying to return to main mode");
      }
    }
    else if (currentRunLevel == 2) {
      if (setupForWifiConection(WiFiSsid, WiFiPassword)) {
        Serial.println("\n===Going for runlevel 3===\n");
        currentRunLevel = 3;
      }
    }
    else if (currentRunLevel == 3) {
      if (SHT30Data.isNeedToSend()) {
        SHT30Data.setlastDataSend(millis());
        waitForSync();
        Timezone Russia;
        Russia.setLocation(timezoneLocation);
        String location = "";
        location = sendGetRequest("http://ip-api.com/json/");

        StaticJsonDocument<1000> doc;
        StaticJsonDocument<1000> part;
        deserializeJson(part, location);

        JsonObject root = doc.to<JsonObject>();
        root["user_id"] = userId;
        root["api_key"] = "5QSXtknjmNsQ4ZnYQ4NsIyoO";
        root["device_local_ip"] = WiFi.localIP().toString();
        root["device_geo_ip"] = part;
        JsonObject loggerData = root.createNestedObject("logger_data");
        loggerData["time"] = Russia.dateTime();
        loggerData["temperature"] = SHT30Data.getAverageTemerature();
        loggerData["humidity"] = SHT30Data.getAverageHumidity();
        String output;
        serializeJson(doc, output);
        sentData(output, logDataUrl);
      }
    }
  }

}
