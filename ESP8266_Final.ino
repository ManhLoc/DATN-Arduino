// Khai báo thư viện cần thiết
#include <Arduino.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

const byte RX = D2;
const byte TX = D1;
SoftwareSerial mySerial = SoftwareSerial(RX, TX);

#define WIFI_SSID "DATN" 
#define WIFI_PASSWORD "maylockhongkhi"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

#define API_KEY "AIzaSyDzt_3d3fQcN8uBMFjiGSCwzsp2M7W27e4"
#define DTBU "https://datn-359b3-default-rtdb.europe-west1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String ChuoiSendEsp = "";
String inputString = "";
bool stringComplete = false;

int Mode = 1;
int Ion = 0;
int Speed = 0;
int Lock = 0;
int Lock2 = 0;
int OnOff = 0;
int OnOff2 = 0;
int Control = 0;
bool signUp = false;

unsigned long last = 0;
float Temp = 0;
float Humid = 0;
int CO2 = 0;
int TVOC = 0;
int PM1 = 0;
int PM25 = 0;
int PM10 = 0;
int AQI_h = 0;

int ttIon = 0;
int ttIon2 = 0;
int ttSpeed = 0;
int ttSpeed2 = 0;
int ttMode = 1;
int ttMode2 = 1;
int ttControl = 0;
int ttControl2 = 0;
String DataSend = "";

// Khai báo biến hẹn giờ
int ClockOnCheck = 0;
int HourOn = 0;
int MinuteOn = 0;
int SecondOn = 0;
int ClockOffCheck = 0;
int HourOff = 0;
int MinuteOff = 0;
int SecondOff = 0;

int currentHour = 0;
int currentMinute = 0;
int currentSecond = 0;

// Lấy Data từ trên Firebase về
void ReadFirebase() {
  if (Firebase.ready() && signUp == true) {
    if (Firebase.RTDB.getInt(&fbdo, "/Control/Mode")) {
      if (fbdo.dataType() == "int") {
        Mode = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/Control/Ion")) {
      if (fbdo.dataType() == "int") {
        Ion = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/Control/Speed")) {  
      if (fbdo.dataType() == "int") {
        Speed = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/Control/Lock")) {
      if (fbdo.dataType() == "int") {
        Lock = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/Control/OnOff")) {
      if (fbdo.dataType() == "int") {
        OnOff = fbdo.intData();
      }
    }

    if((OnOff2 != OnOff) || (Lock2 != Lock)) {
      OnOff2 = OnOff;
      Lock2 = Lock;

      if (OnOff == 0 && Lock == 0) {
        Control = 0;
      } else if (OnOff == 0 && Lock == 1) {
        Control = 1;
      } else if (OnOff == 1 && Lock == 0) {
        Control = 2;  
      } else if (OnOff == 1 && Lock == 1) {
        Control = 3;
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/ClockOn/OnOff")) {
      if (fbdo.dataType() == "int") {
        ClockOnCheck = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/ClockOn/Hour")) {
      if (fbdo.dataType() == "int") {
        HourOn = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/ClockOn/Minute")) {
      if (fbdo.dataType() == "int") {
        MinuteOn = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/ClockOn/Second")) {
      if (fbdo.dataType() == "int") {
        SecondOn = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/ClockOff/OnOff")) {
      if (fbdo.dataType() == "int") {
        ClockOffCheck = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/ClockOff/Hour")) {
      if (fbdo.dataType() == "int") {
        HourOff = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/ClockOff/Minute")) {
      if (fbdo.dataType() == "int") {
        MinuteOff = fbdo.intData();
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/ClockOff/Second")) {
      if (fbdo.dataType() == "int") {
        SecondOff = fbdo.intData();
      }
    }
  } else {
  }
}

// Gửi Data qua cho ATMega
void RequestDataMega() {
  DataSend = "";
  //DataSend = "{\"SEND\":\"" + String("1") + "\"}";
  // DataSend = "{\"SEND\":\"1\"}";

  DataSend = "{\"SEND\":\"" + String("1") + "\"," + "\"Mode\":\"" + String(Mode) + "\"," + "\"Ion\":\"" + String(Ion) + "\"," + "\"Control\":\"" + String(Control) + "\"," + "\"Speed\":\"" + String(Speed) + "\"}";

  StaticJsonDocument<256> doc;
  deserializeJson(doc, DataSend);

  serializeJsonPretty(doc, mySerial);
  mySerial.flush();

  Serial.println("Data gửi Mega: ");
  serializeJson(doc, Serial);
  Serial.println();

  doc.clear();
}

// Nhận Data từ ATMega để đẩy Data lên Firebase
void Read_UART_JSON() {
  while (mySerial.available()) {
    const size_t capacity = JSON_OBJECT_SIZE(2) + 300;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, mySerial);
    if (error) {
      return;
    } else {
      Serial.println("Data nhận Mega: ");
      serializeJson(doc, Serial);
      Serial.println();
      //======================================================
      if (doc.containsKey("TTControl") > 0) {
        ttControl = doc["TTControl"];
        if (ttControl2 != ttControl) {
          ttControl2 = ttControl;
          
          if (ttControl == 0) {
            Control = 0;
            if (WiFi.status() == WL_CONNECTED) { 
              Firebase.RTDB.setInt(&fbdo, "/Control/OnOff", 0);
              Firebase.RTDB.setInt(&fbdo, "/Control/Lock", 0);
            }
          } else if (ttControl == 1) {
            Control = 1;
            if (WiFi.status() == WL_CONNECTED) { 
              Firebase.RTDB.setInt(&fbdo, "/Control/OnOff", 0);
              Firebase.RTDB.setInt(&fbdo, "/Control/Lock", 1);
            }
          } else if (ttControl == 2) {
            Control = 2;
            if (WiFi.status() == WL_CONNECTED) { 
              Firebase.RTDB.setInt(&fbdo, "/Control/OnOff", 1);
              Firebase.RTDB.setInt(&fbdo, "/Control/Lock", 0);
            }
          } else if (ttControl == 3) {
            Control = 3;
            if (WiFi.status() == WL_CONNECTED) { 
              Firebase.RTDB.setInt(&fbdo, "/Control/OnOff", 1); 
              Firebase.RTDB.setInt(&fbdo, "/Control/Lock", 1);
            }
          }
        }
      }

      if (doc.containsKey("TTMode") > 0) {
        ttMode = doc["TTMode"];
        if (ttMode2 != ttMode) {
          ttMode2 = ttMode;

          if (WiFi.status() == WL_CONNECTED) { 
            Firebase.RTDB.setInt(&fbdo, "/Control/Mode", ttMode);
          } else {
            Mode = ttMode;
          }
        }
      }

      if (doc.containsKey("TTIon") > 0) {
        ttIon = doc["TTIon"];
        if (ttIon2 != ttIon) {
          ttIon2 = ttIon;

          if (WiFi.status() == WL_CONNECTED) { 
            Firebase.RTDB.setInt(&fbdo, "/Control/Ion", ttIon);
          } else {
            Ion = ttIon;
          }
        }
      }

      if (doc.containsKey("TTSpeed") > 0) {
        ttSpeed = doc["TTSpeed"];
        if (ttSpeed2 != ttSpeed) {
          ttSpeed2 = ttSpeed;

          if (WiFi.status() == WL_CONNECTED) { 
            Firebase.RTDB.setInt(&fbdo, "/Control/Speed", ttSpeed);
          } else {
            Speed = ttSpeed;
          }
        }
      }

      if (doc.containsKey("TEMP") < 1000) {
        Temp = doc["TEMP"];
        if (WiFi.status() == WL_CONNECTED) { 
          Firebase.RTDB.setInt(&fbdo, "/Sensor/Temp", Temp);
        }
      }

      if (doc.containsKey("HUMID") < 1000) {
        Humid = doc["HUMID"];
        if (WiFi.status() == WL_CONNECTED) { 
          Firebase.RTDB.setInt(&fbdo, "/Sensor/Humid", Humid);
        }
      }

      if (doc.containsKey("CO2") < 6000) {
        CO2 = doc["CO2"];
        if (WiFi.status() == WL_CONNECTED) { 
          Firebase.RTDB.setInt(&fbdo, "/Sensor/CO2", CO2);
        }
      }

      if (doc.containsKey("TVOC") < 1500) {
        TVOC = doc["TVOC"];
        if (WiFi.status() == WL_CONNECTED) { 
          Firebase.RTDB.setInt(&fbdo, "/Sensor/TVOC", TVOC);
        }
      }

      if (doc.containsKey("PM1") < 1000) {
        PM1 = doc["PM1"];
        if (WiFi.status() == WL_CONNECTED) { 
          Firebase.RTDB.setInt(&fbdo, "/Sensor/PM1", PM1);
        }
      }

      if (doc.containsKey("PM2.5") < 1000) {
        PM25 = doc["PM2.5"];
        if (WiFi.status() == WL_CONNECTED) { 
          Firebase.RTDB.setInt(&fbdo, "/Sensor/PM25", PM25);
        }
      }

      if (doc.containsKey("PM10") < 1000) {
        PM10 = doc["PM10"];
        if (WiFi.status() == WL_CONNECTED) { 
          Firebase.RTDB.setInt(&fbdo, "/Sensor/PM10", PM10);
        }
      }

      if (doc.containsKey("AQI_h") < 1000) {
        AQI_h = doc["AQI_h"];
        if (WiFi.status() == WL_CONNECTED) { 
          Firebase.RTDB.setInt(&fbdo, "/Sensor/AQI_h", AQI_h);
        }
      }
      //======================================================

      char buffer[500];
      size_t n = serializeJson(doc, buffer);
      //client.publish(topicpub.c_str(), buffer, n);

      for (int i = 0; i < 500; i++) {
        buffer[i] = 0;
      }

      doc.clear();
    }
  }
}

void ClockOnOff() {
  timeClient.update();
  currentHour = timeClient.getHours();
  currentMinute = timeClient.getMinutes();
  currentSecond = timeClient.getSeconds();

  // Bật thiết bị
  if(ClockOnCheck == 1) {
    if(HourOn == currentHour && MinuteOn == currentMinute) {
      Firebase.RTDB.setInt(&fbdo, "/Control/OnOff", 1);
    }
  }

  // Tắt thiết bị
  if(ClockOffCheck == 1) {
    if(HourOff == currentHour && MinuteOff == currentMinute) {
      Firebase.RTDB.setInt(&fbdo, "/Control/OnOff", 0);
    }
  }
}

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  last = millis();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  // WiFi.setAutoReconnect(true);
  // WiFi.persistent(true);

  config.api_key = API_KEY;
  config.database_url = DTBU;

  if (Firebase.signUp(&config, &auth, "", "")) {
    signUp = true;
  } else {
  }
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Firebase.RTDB.setInt(&fbdo, "/Control/OnOff", 0);
  Firebase.RTDB.setInt(&fbdo, "/Control/Mode", ttMode);

  timeClient.begin();
  timeClient.setTimeOffset(25200);
}

void loop() { 
  if (millis() - last >= 500) {
    if (WiFi.status() == WL_CONNECTED) { 
      ReadFirebase();
    } else {
    }
    RequestDataMega();
    last = millis();
  }
  Read_UART_JSON();
  ClockOnOff();
}