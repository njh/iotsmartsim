#include <MKRGSM.h>
#include <ArduinoUniqueID.h>
#include <ArduinoJson.h>
#include "safe2.h"

// LED (Config)
#define PIN_LED  7

Safe2 safe2(&SerialGSM, NULL);

#define STATE_READY  0x00
#define STATE_REQUEST_SENT  0x01
#define STATE_REQUEST_DATA  0x02
#define STATE_DONE  0x03

#define RES_JSON_DESERIALIZATION_FAILED  0x81

#define MODEM_BAUD_RATE  9600
#define LOG_BAUD_RATE  115200

#define LEN_DEVICE_ID_MAX  16
unsigned char id[LEN_DEVICE_ID_MAX];

char gState;

#define JSON_CONFIG_CAPACITY  192
StaticJsonDocument<JSON_CONFIG_CAPACITY> cfg;

#define LEN_CONFIG  255
byte gConfig[LEN_CONFIG];

#define LEN_NAME_CONTROL_LED  9
const char NAME_CONTROL_LED[LEN_NAME_CONTROL_LED] = { 'i', 'o', 't', ':', 'A', 'l', 'a', 'r', 'm' };

#define LEN_NAME_EFFECT_ON  2
const char NAME_EFFECT_ON[LEN_NAME_EFFECT_ON] = { 'O', 'n' };

char configParseApply(const char * buf, short dataLen) {
  
  DeserializationError error = deserializeJson(cfg, buf, dataLen);
  
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return RES_JSON_DESERIALIZATION_FAILED;
  }
  
  const char* cfg_version = cfg["configuration"]["version"]; // "2020-10-13"
  Serial.print("configuration version: ");
  Serial.println(cfg_version);
  
  const char* cfg_config_0_action = cfg["configuration"]["config"][0]["action"]; // "iot:Alarm"
  const char* cfg_config_0_effect = cfg["configuration"]["config"][0]["effect"]; // "On"

  if (memcmp(cfg_config_0_action, NAME_CONTROL_LED, LEN_NAME_CONTROL_LED) == 0) {
    if (memcmp(cfg_config_0_effect, NAME_EFFECT_ON, LEN_NAME_EFFECT_ON) == 0) {
      Serial.println("iot:Alarm configured as ON");
      digitalWrite(PIN_LED, HIGH);
    } else {
      Serial.println("iot:Alarm configured as OFF");
      digitalWrite(PIN_LED, LOW);
    }
  }
  
}

void setup() {
  byte res;

  // configure pins
  pinMode(PIN_LED, OUTPUT);
  Serial.begin(LOG_BAUD_RATE);

  safe2.init(MODEM_BAUD_RATE, 0);

  Serial.println("waiting for modem start...");
  safe2.waitForModemStart();
  Serial.println("OK");

  Serial.println("waiting for network registration...");
  safe2.waitForNetworkRegistration();
  Serial.println("OK");
  Serial.println("setup finished");

  
  // put Device ID
  short idLen = UniqueIDsize;
  if (idLen > LEN_DEVICE_ID_MAX) {
    idLen = LEN_DEVICE_ID_MAX;
  }
  memcpy(id, (void *)UniqueID, idLen);
  res = safe2.deviceIdSet(id, idLen);
  if (res == RES_OK) {
    Serial.println("Set Device ID: OK");
  } else {
    Serial.println("Set Device ID: ERROR");
  }
  safe2.prepareForSleep();
  
  gState = STATE_READY;
}

void loop() {

  byte secs;
  short dataLen = 0;
  short receiveState, receiveRes;
  char res;


  if (gState == STATE_READY) {
    Serial.println("config request");
    res = safe2.configRequest();
    if (res == RES_OK) {
      Serial.println("request - OK");
      gState = STATE_REQUEST_SENT;
    } else {
      gState = STATE_DONE;
    }
  }
  if (gState == STATE_REQUEST_SENT) {
    
    for (secs=0; secs < 10; secs++) {
      delay(1000);
    }
    res = safe2.state(receiveState, receiveRes);
    Serial.print("receiving state: ");
    Serial.println(receiveState, HEX);
    if (receiveState == STATUS_RECEIVE_DATA) {
      gState = STATE_REQUEST_DATA;
    }
    if (receiveState == STATUS_RECEIVE_DONE) {
      if (receiveRes != 0) {
        Serial.print("receiving error: ");
        Serial.println(receiveRes, HEX);
        gState = STATE_DONE;  
        return;
      }
    }
    if (res != RES_OK) {
      gState = STATE_DONE;
      return;
    }
  }
  if (gState == STATE_REQUEST_DATA) {
    dataLen = LEN_CONFIG;
    res = safe2.configGet(gConfig, dataLen);
    if (res == RES_OK) {
      res = configParseApply((char *)gConfig, dataLen);
    }
    gState = STATE_DONE;
  }
  if (gState == STATE_DONE) {
    safe2.prepareForSleep();
    gState = STATE_READY;
  
    Serial.print("waiting");
    byte minutes = 5;
    
    while (minutes > 0) {
      for (secs=0; secs < 60; secs++) {
        delay(1000);
      }
      minutes -= 1;
      Serial.print('.');
    }
  }
  Serial.println();
}
