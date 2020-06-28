#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#define _SSID "VLUTE FIT IoT"
#define _PASSWORD "fit@2020"
#define _MQTT_SERVER "nhamang.vlute.edu.vn"
#define _MQTT_PORT 1883
#define _PIN_STATUS 16

WiFiClient _ESP_CLIENT;
PubSubClient _CLIENT(_ESP_CLIENT);
SoftwareSerial ModbusSerial(D2, D3); // RXD, TXD
byte RxData[7] ;
const byte TxData[9] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};

char _MQTT_ID[50];
char _CHANNEL_PH[50];
long TIMEOUT_PUBLISH = 0;
long TIMEOUT_LED_STATUS = 0;
unsigned long waitTime = 3000;
void OnOff(int pin, int st) {
  if (st == 0 && digitalRead(pin) == LOW)
    digitalWrite(pin, HIGH);
  else if (st == 1 && digitalRead(pin) == HIGH)
    digitalWrite(pin, LOW);
}

String getChipID() {
  return "SENSOR-" + String(ESP.getChipId());
}

void Wifi() {
  Serial.println("Connecting to " + String(_SSID));
  Serial.println(getChipID());
  WiFi.begin(_SSID, _PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

float ReadPH() {
  //Send Request Cmt
  ModbusSerial.write(TxData, 8);
  //Read Response
  if (ModbusSerial.available())
  {
    for (int i = 0; i < sizeof(RxData); i++) {
      RxData[i] = ModbusSerial.read();
    }
  }
  else
  {
    Serial.println("Khong tim duoc cam bien");
  }
  float PH = (RxData[3] * 256 + RxData[4]) / 100.0;
  delay(50); // delay for waiting get all Data
  return PH;
}

void setup() {
  Serial.begin(115200);
  ModbusSerial.begin(9600);
  pinMode(_PIN_STATUS, OUTPUT);
  // Cấu hình MQTT
  String chipID = getChipID();
  String CHANNEL_PH = String("PH/" + chipID);
  Serial.println(CHANNEL_PH);
  chipID.toCharArray(_MQTT_ID, chipID.length() + 1);
  CHANNEL_PH.toCharArray(_CHANNEL_PH, CHANNEL_PH.length() + 1);
  _CLIENT.setServer(_MQTT_SERVER, _MQTT_PORT);
}

void ReconnectMQTT() {
  while (!_CLIENT.connected() &&  WiFi.status() == WL_CONNECTED) {
    if (_CLIENT.connect(_MQTT_ID)) {
      Serial.println("MQTT is connected..." );
      _CLIENT.subscribe(_CHANNEL_PH);
    } else {
      OnOff(_PIN_STATUS, 0);
      Serial.print("failed, rc=");
      Serial.print(_CLIENT.state());
      Serial.println("Try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  long now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Wifi();
    _CLIENT.setServer(_MQTT_SERVER, _MQTT_PORT);

  } else {
    if (!_CLIENT.connected()) {
      ReconnectMQTT();
    } else {
      _CLIENT.loop();
    }
  }

  // Thông báo led
  if (now - TIMEOUT_LED_STATUS > 2000) {
    TIMEOUT_LED_STATUS = now;
    if (WiFi.status() == WL_CONNECTED) {
      if (digitalRead(_PIN_STATUS) == HIGH) {
        OnOff(_PIN_STATUS, 1);
      } else {
        OnOff(_PIN_STATUS, 0);
      }
    }
  }

  // Gửi dữ liệu
  if (now - TIMEOUT_PUBLISH > 5 *1000) {
    TIMEOUT_PUBLISH = now;
    float PH = ReadPH();
    _CLIENT.publish(_CHANNEL_PH, String(PH).c_str());
    Serial.println(PH);
}
}
