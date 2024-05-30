// Khai báo thư viện cần thiết
#include <PMS.h>
#include <Wire.h>
#include <Servo.h>
#include <DS3231.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_SHT31.h>
#include <SparkFunCCS811.h>

// Khai báo biến kiểm tra đủ 3 tiếng chưa 
bool check = true;
int dem = 0;

// Khai báo quạt
int FAN_PIN = 2;
int inPut = 3;

// Khai báo buzzer
int buzzer = 53;

// Khai báo relay
int relay = 51;

// Khai báo PMS
PMS pms(Serial3);
PMS::DATA data;

// Định nghĩa địa chỉ I2C cho CCS811
#define CCS811_ADDR 0x5A

String ChuoiSendEsp = "";
String inputString = "";
bool stringComplete = false;

unsigned long last = 0;

// Khai báo các biến để nhận data từ ESP
int Mode = 1;
int ssMode = 1;
int Ion = 0;
int ssIon = 0;
int Speed = 0;
int ssSpeed = 0;
int Control = 0;
int ssControl = 0;

// Khai báo biến truyền data qua ESP khi HMI được nhấn
int ttMode = 1;
int ttControl = 0;
int ttIon = 0;
int ttSpeed = 0;

// Khai báo chân điều LED
#define DKLED 50

// Khai báo cảm biến MP-135
int mp135 = A1;
int giatrimp135 = 0;

// Khai báo cảm biến SHT31
float Temp = 0;
float Humid = 0;
bool enableHeater = false;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Khai báo cảm biến CCS811
int CO2 = 0;
int TVOC = 0;
CCS811 mySensor(CCS811_ADDR);

int PM1 = 0;
int PM25 = 0;
int PM10 = 0;
float sum_sec_PM1 = 0;
float sum_sec_PM25 = 0;
float sum_sec_PM10 = 0;
float sum_min_PM1 = 0;
float sum_min_PM25 = 0;
float sum_min_PM10 = 0;
int AQI_h = 0;

float input_PM25[24] = { 20.2, 15.15, 16.38, 18.97, 18.72, 25.7, 19.9, 20.2, 20.83, 27.45, 33.2, 33.17, 27.9, 8.37, 9.8, 21.75, 16.12, 17.05, 13.52, 16.78, 18.83, 19.58, 25.4, 19.77 };
float inputAQI_h[24] = { 0 };

// Khai báo DS3231
DS3231 rtc(SDA, SCL);
Time time;

int sec = 0;
int minute = 0;
int pre_sec = 0;
int pre_minute = 0;
int count_sec = 0;
int count_min = 0;

// Tính toán AQI và xử lý
void shiftArray(float arr[], float newValue) {
  for (int i = 0; i < 23; i++) {
    arr[i] = arr[i + 1];
  }
  arr[23] = newValue;
}

float findMin(float arr[]) {
  float min = arr[0];
  for (int i = 1; i < 12; i++) {
    if (arr[i] < min) {
      min = arr[i];
    }
  }
  return min;
}

float findMax(float arr[]) {
  float max = arr[0];
  for (int i = 1; i < 12; i++) {
    if (arr[i] > max) {
      max = arr[i];
    }
  }
  return max;
}

float calculate_nowcast(float arr[]) {
  float w;
  float nowcast = 0;
  float sum1 = 0;
  float sum2 = 0;
  float input_nowcast[12];
  for (int i = 0; i < 12; i++) {
    input_nowcast[i] = arr[i + 12];
  }
  float cmax = findMax(input_nowcast);
  float cmin = findMin(input_nowcast);
  w = (float)cmin / cmax;
  if (w <= 0.5) {
    w = 0.5;
    for (int i = 0; i < 12; i++) {
      nowcast = nowcast + (pow(w, i + 1) * input_nowcast[12 - i - 1]);
    }
  }
  if (w > 0.5) {
    for (int i = 0; i < 12; i++) {
      sum1 += input_nowcast[12 - i - 1] * pow(w, (i + 1) - 1);
      sum2 += pow(w, (i + 1) - 1);
    }
    nowcast = sum1 / sum2;
  }
  return nowcast;
}

float calculateAQI(float value) {
  int AQI;
  int ni, nj, a, b;
  if (value >= 0 && value < 25) {
    a = 0;
    b = 25;
    ni = 0;
    nj = 50;
  } else if (value >= 25 && value < 50) {
    a = 25;
    b = 50;
    ni = 50;
    nj = 100;
  } else if (value >= 50 && value < 80) {
    a = 50;
    b = 80;
    ni = 100;
    nj = 150;
  } else if (value >= 80 && value < 150) {
    a = 80;
    b = 150;
    ni = 150;
    nj = 200;
  } else if (value >= 150 && value < 250) {
    a = 150;
    b = 250;
    ni = 200;
    nj = 300;
  } else if (value >= 250 && value < 350) {
    a = 250;
    b = 350;
    ni = 300;
    nj = 400;
  } else if (value >= 350 && value < 500) {
    a = 350;
    b = 500;
    ni = 400;
    nj = 500;
  }
  AQI = (((nj - ni) / (b - a)) * (value - a)) + ni;
  return AQI;
}

// Truyền Data qua ESP
void DataJson(String ttMode, String Temp, String Humid, String CO2, String TVOC, String PM1, String PM25, String PM10, String ttControl, String ttIon, String ttSpeed, String AQI_h) {
  ChuoiSendEsp = "";
  ChuoiSendEsp = "{\"TTMode\":\"" + String(ttMode) + 
  "\"," + "\"TEMP\":\"" + String(Temp) + 
  "\"," + "\"HUMID\":\"" + String(Humid) + 
  "\"," + "\"CO2\":\"" + String(CO2) + 
  "\"," + "\"TVOC\":\"" + String(TVOC) + 
  "\"," + "\"PM1\":\"" + String(PM1) + 
  "\"," + "\"PM2.5\":\"" + String(PM25) + 
  "\"," + "\"PM10\":\"" + String(PM10) + 
  "\"," + "\"TTControl\":\"" + String(ttControl) + 
  "\"," + "\"TTIon\":\"" + String(ttIon) + 
  "\"," + "\"TTSpeed\":\"" + String(ttSpeed) + 
  "\"," + "\"AQI_h\":\"" + String(AQI_h) + "\"}";

  // Dua du lieu vao thu vien ArduinoJSON
  StaticJsonDocument<256> doc;
  deserializeJson(doc, ChuoiSendEsp);

  Serial.println("ChuoiSendEsp: ");
  serializeJsonPretty(doc, Serial);
  Serial.println();

  serializeJsonPretty(doc, Serial1);
  Serial1.flush();

  doc.clear();
}

// Nhận Data từ ESP để điều khiển thiết bị
void Read_UART_JSON() {
  while (Serial1.available()) {

    const size_t capacity = JSON_OBJECT_SIZE(2) + 300;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, Serial1);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    } else {
      Serial.println("Data nhận ESP: ");
      serializeJson(doc, Serial);
      Serial.println();

      if (doc.containsKey("Mode") > 0) {
        Mode = doc["Mode"];
        // chonchedo(Mode);
        if ((ssMode != Mode) && Mode == 1) {
          ttMode = 1;
          ssMode = Mode;

          if(Control == 2) {
            Serial2.print("page Home");
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          } else if (Control == 3) {
            Serial2.print("page HomeLock");
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          }

          Serial2.print("bt1.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt2.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt0.val=");
          Serial2.print(1);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        } else if ((ssMode != Mode) && Mode == 2) {
          ttMode = 2;
          ssMode = Mode;
          Serial2.print("bt0.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt2.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt1.val=");
          Serial2.print(1);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("page HomeNormal");
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          if(ttIon == 0) {
            Serial2.print("bt3.val=");
            Serial2.print(0);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          } else {
            Serial2.print("bt3.val=");
            Serial2.print(1);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          }

          if(ttSpeed == 0) {
            Serial2.print("h0.val=");
            Serial2.print(25);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          } else if(ttSpeed == 1) {
            Serial2.print("h0.val=");
            Serial2.print(50);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          } else if(ttSpeed == 2) {
            Serial2.print("h0.val=");
            Serial2.print(75);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          } else if(ttSpeed == 3) {
            Serial2.print("h0.val=");
            Serial2.print(100);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          }

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        } else if ((ssMode != Mode) && Mode == 3) {
          ttMode = 3;
          ssMode = Mode;

          if(Control == 2) {
            Serial2.print("page Home");
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          } else if (Control == 3) {
            Serial2.print("page HomeLock");
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          }

          Serial2.print("bt0.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt1.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt2.val=");
          Serial2.print(1);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        }
      }

      if (doc.containsKey("Speed") >= 0) {
        Speed = doc["Speed"];
        if ((ssSpeed != Speed) && Speed == 0) {
          ttSpeed = 0;
          ssSpeed = Speed;

          Serial2.print("h0.val=");
          Serial2.print(25);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        } else  if ((ssSpeed != Speed) && Speed == 1) {
          ttSpeed = 1;
          ssSpeed = Speed;

          Serial2.print("h0.val=");
          Serial2.print(50);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        } else  if ((ssSpeed != Speed) && Speed == 2) {
          ttSpeed = 2;
          ssSpeed = Speed;

          Serial2.print("h0.val=");
          Serial2.print(75);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        } else  if ((ssSpeed != Speed) && Speed == 3) {
          ttSpeed = 3;
          ssSpeed = Speed;

          Serial2.print("h0.val=");
          Serial2.print(100);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        }
      }

      if (doc.containsKey("Ion") >= 0) {
        Ion = doc["Ion"];
        if ((ssIon != Ion) && Ion == 0) {
          ttIon = 0;
          ssIon = Ion;

          Serial2.print("bt3.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        } else  if ((ssIon != Ion) && Ion == 1) {
          ttIon = 1;
          ssIon = Ion;

          Serial2.print("bt3.val=");
          Serial2.print(1);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        }
      }

      if (doc.containsKey("Control") >= 0) {
        Control = doc["Control"]; 

        if ((ssControl != Control) && Control == 0) {
          ssControl = Control;
          ttControl = 0;

          Serial2.print("page Logo");
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        } else if ((ssControl != Control) && Control == 1) {
          ssControl = Control;
          ttControl = 1;

          Serial2.print("page LogoLock");
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        } else if ((ssControl != Control) && Control == 2) {
          ssControl = Control;
          ttControl = 2;

          if(ttMode == 2) {
            Serial2.print("page HomeNormal");
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);

            if(ttIon == 0) {
              Serial2.print("bt3.val=");
              Serial2.print(0);
              Serial2.write(0xff);
              Serial2.write(0xff);
              Serial2.write(0xff);
            } else {
              Serial2.print("bt3.val=");
              Serial2.print(1);
              Serial2.write(0xff);
              Serial2.write(0xff);
              Serial2.write(0xff);
            }

            if(ttSpeed == 0) {
              Serial2.print("h0.val=");
              Serial2.print(25);
              Serial2.write(0xff);
              Serial2.write(0xff);
              Serial2.write(0xff);
            } else if(ttSpeed == 1) {
              Serial2.print("h0.val=");
              Serial2.print(50);
              Serial2.write(0xff);
              Serial2.write(0xff);
              Serial2.write(0xff);
            } else if(ttSpeed == 2) {
              Serial2.print("h0.val=");
              Serial2.print(75);
              Serial2.write(0xff);
              Serial2.write(0xff);
              Serial2.write(0xff);
            } else if(ttSpeed == 3) {
              Serial2.print("h0.val=");
              Serial2.print(100);
              Serial2.write(0xff);
              Serial2.write(0xff);
              Serial2.write(0xff);
            }
          } else {
            Serial2.print("page Home");
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          }

          if (Mode == 1) {
            Serial2.print("bt1.val=");
            Serial2.print(0);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);

            Serial2.print("bt2.val=");
            Serial2.print(0);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);

            Serial2.print("bt0.val=");
            Serial2.print(1);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          } else if (Mode == 2) {
            Serial2.print("bt0.val=");
            Serial2.print(0);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);

            Serial2.print("bt2.val=");
            Serial2.print(0);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);

            Serial2.print("bt1.val=");
            Serial2.print(1);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          } else if (Mode == 3) {
            Serial2.print("bt0.val=");
            Serial2.print(0);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);

            Serial2.print("bt1.val=");
            Serial2.print(0);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);

            Serial2.print("bt2.val=");
            Serial2.print(1);
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
          }

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        } else if ((ssControl != Control) && Control == 3) {
          ssControl = Control;
          ttControl = 3;

          Serial2.print("page HomeLock");
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
        }
      }

      // Xử lý dữ liệu khi nhận được Data từ ESP
      if (doc["SEND"] == "1") {
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));
      }

      doc.clear();
    }
  }
}

// Điều khiển LED từ HMI
void DieuKhienHMI() {
  byte value = 0xff;
  while (Serial2.available()) {
    value = Serial2.read();
    switch (value) {
      case 0x10:
        ttMode = 1;
        if(Control == 2) {
          ttControl = 2;
        } else if (Control == 3) {
          ttControl = 3;
        }
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x11:
        ttMode = 2;
        if(Control == 2) {
          ttControl = 2;
        } else if (Control == 3) {
          ttControl = 3;
        }

        Serial2.print("page HomeNormal");
        Serial2.write(0xff);
        Serial2.write(0xff);
        Serial2.write(0xff);

        if(ttIon == 0) {
          Serial2.print("bt3.val=");za
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);
        } else {
          Serial2.print("bt3.val=");
          Serial2.print(1);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);
        }

        if(ttSpeed == 0) {
          Serial2.print("h0.val=");
          Serial2.print(25);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);
        } else if(ttSpeed == 1) {
          Serial2.print("h0.val=");
          Serial2.print(50);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);
        } else if(ttSpeed == 2) {
          Serial2.print("h0.val=");
          Serial2.print(75);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);
        } else if(ttSpeed == 3) {
          Serial2.print("h0.val=");
          Serial2.print(100);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);
        }

        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x12:
        ttMode = 3;
        if(Control == 2) {
          ttControl = 2;
        } else if (Control == 3) {
          ttControl = 3;
        }
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x13:
        ttControl = 2;
        Control = 2;
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        if (Mode == 1) {
          Serial2.print("bt1.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt2.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt0.val=");
          Serial2.print(1);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);
        } else if (Mode == 2) {
          Serial2.print("bt0.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt2.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt1.val=");
          Serial2.print(1);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);
        } else if (Mode == 3) {
          Serial2.print("bt0.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt1.val=");
          Serial2.print(0);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);

          Serial2.print("bt2.val=");
          Serial2.print(1);
          Serial2.write(0xff);
          Serial2.write(0xff);
          Serial2.write(0xff);
        }

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x14:
        ttControl = 0;
        Control = 0;
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x20:
        ttSpeed = 0;
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x21:
        ttSpeed = 1;
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x22:
        ttSpeed = 2;
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x23:
        ttSpeed = 3;
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x24:
        ttIon = 0;
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x25:
        ttIon = 1;
        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
      case 0x26:
        Serial2.print("bt0.val=");
        Serial2.print(0);
        Serial2.write(0xff);
        Serial2.write(0xff);
        Serial2.write(0xff);

        Serial2.print("bt2.val=");
        Serial2.print(0);
        Serial2.write(0xff);
        Serial2.write(0xff);
        Serial2.write(0xff);

        Serial2.print("bt1.val=");
        Serial2.print(1);
        Serial2.write(0xff);
        Serial2.write(0xff);
        Serial2.write(0xff);

        DataJson(String(ttMode), String(Temp), String(Humid), String(CO2), String(TVOC), String(PM1), String(PM25), String(PM10), String(ttControl), String(ttIon), String(ttSpeed), String(AQI_h));

        digitalWrite(buzzer, HIGH);
        delay(200);
        digitalWrite(buzzer, LOW);
        break;
    }
  }
}

void UpdateValueHMI() {
  Serial2.print("pm25V.val=");
  Serial2.print(PM25);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);

  Serial2.print("tvocV.val=");
  Serial2.print(TVOC);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);

  Serial2.print("co2V.val=");
  Serial2.print(CO2);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);

  int TempV = Temp;
  Serial2.print("tempV.val=");
  Serial2.print(TempV);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);

  int humV = Humid;
  Serial2.print("humV.val=");
  Serial2.print(humV);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
}

void ChonCheDo() {
  if (ttMode == 1) {
    // Chế độ Auto
    // 3 tiếng bật tắt Ion 1 lần
    if(check) {
      digitalWrite(relay, HIGH);
    } else {
      digitalWrite(relay, LOW);
    }
    if (PM1 >= 36 || PM25 >= 36 || PM10 >= 155 || TVOC >= 661 || giatrimp135 >= 1000) {
      analogWrite(FAN_PIN, 255);
    } else if ((PM1 >= 13 && PM1 <= 35) || (PM25 >= 13 && PM25 <= 35) || (PM10 >= 55 && PM10 <= 154) || (TVOC >= 221 && TVOC <= 660) || (giatrimp135 >= 547 && giatrimp135 < 1000)) {
      // Tốc độ quạt tb
      analogWrite(FAN_PIN, 150);
    } else {
      analogWrite(FAN_PIN, 50);
    }
  } else if (ttMode == 2) {
    // Chế độ Normal
    if (ttSpeed == 0) {
      analogWrite(FAN_PIN, 0);
    } else if (ttSpeed == 1) {
      analogWrite(FAN_PIN, 50);
    } else if (ttSpeed == 2) {
      analogWrite(FAN_PIN, 150);
    } else if (ttSpeed == 3) {
      analogWrite(FAN_PIN, 255);
    }
    // Bật tắt Ion
    if (ttIon == 0) {
      digitalWrite(relay, LOW);
    } else {
      digitalWrite(relay, HIGH);
    }
  } else {
    // Chế độ Sleep
    analogWrite(FAN_PIN, 50);
    digitalWrite(relay, LOW);
  }
}

void setup() {
  rtc.begin();
  Wire.begin();

  Serial.begin(9600);
  Serial1.begin(9600);
  Serial2.begin(9600);
  Serial3.begin(9600);

  // Trang chủ màn hình
  Serial2.print("page Logo");
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);

  // Khai báo ngõ ra  MP-135
  pinMode(mp135, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(relay, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(inPut, OUTPUT);
  digitalWrite(inPut, HIGH);
  last = millis();

  // Kiểm tra trạng thái của SHT31
  if (!sht31.begin(0x44)) {  // Set to 0x45 for alternate i2c addr
    while (1)
      delay(1);
  }

  if (mySensor.begin() == false) {
    while (1)
      ;
  }

  time = rtc.getTime();
  sec = time.sec;
  minute = time.min;
  pre_sec = sec;
  pre_minute = minute;

  delay(1000);
}

void loop() {
  Read_UART_JSON();

  time = rtc.getTime();
  sec = time.sec;
  minute = time.min;

  if (ttControl == 0 || ttControl == 1) {
    digitalWrite(relay, LOW);
    analogWrite(FAN_PIN, 0);
  } else if (ttControl == 2 || ttControl == 3) {
    if (pms.read(data) && (millis() - last >= 2000 || last == 0)) {
      giatrimp135 = analogRead(mp135);

      Temp = sht31.readTemperature();
      Humid = sht31.readHumidity();

      if (!isnan(Temp)) {

      } else {
      }

      if (!isnan(Humid)) {

      } else {
      }

      if (mySensor.dataAvailable()) {
        mySensor.readAlgorithmResults();
        CO2 = mySensor.getCO2();
        TVOC = mySensor.getTVOC();
      }

      if (pms.read(data)) {
        Serial1.print("PM 1.0 (ug/m3): ");
        Serial1.println(data.PM_AE_UG_1_0);

        Serial1.print("PM 2.5 (ug/m3): ");
        Serial1.println(data.PM_AE_UG_2_5);

        Serial1.print("PM 10.0 (ug/m3): ");
        Serial1.println(data.PM_AE_UG_10_0);

        Serial1.println();
      }
      // Đọc giá trị các cảm biến
      last = millis();

      PM1 = data.PM_AE_UG_1_0;
      PM25 = data.PM_AE_UG_2_5;
      PM10 = data.PM_AE_UG_10_0;

      count_sec += 1;

      sum_sec_PM1 += PM1;
      sum_sec_PM25 += PM25;
      sum_sec_PM10 += PM10;

      // Mỗi 1p trôi qua tính tb rồi reset tổng giây
      if (pre_minute != minute) {

        pre_minute = minute;
        count_min += 1;

        sum_sec_PM1 = sum_sec_PM1 / count_sec;
        sum_sec_PM25 = sum_sec_PM25 / count_sec;
        sum_sec_PM10 = sum_sec_PM10 / count_sec;

        sum_min_PM1 += sum_sec_PM1;
        sum_min_PM25 += sum_sec_PM25;
        sum_min_PM10 += sum_sec_PM10;

        count_sec = 0;
        sum_sec_PM1 = 0;
        sum_sec_PM25 = 0;
        sum_sec_PM10 = 0;

        // 1h đã trôi qua
        if (minute == 0) {
          // Đếm 3 tiếng đổi trạng thái 1 lần 
          if(Mode == 1) {
            dem++;
            if(dem == 3) {
              dem = 0;
              check = !check;
            }
          }
          // Tính tb của 1h
          sum_min_PM1 /= count_min;
          sum_min_PM25 /= count_min;
          sum_min_PM10 /= count_min;

          // Thêm giá trị mới tính vô mảng
          shiftArray(input_PM25, sum_min_PM25);

          // Tính giá trị AQI giờ
          AQI_h = calculateAQI(calculate_nowcast(input_PM25));
          shiftArray(inputAQI_h, AQI_h);

          count_min = 0;
          sum_min_PM1 = 0;
          sum_sec_PM25 = 0;
          sum_sec_PM10 = 0;
        }
      }
      UpdateValueHMI();
      ChonCheDo();
    } 
  }
  DieuKhienHMI();
}