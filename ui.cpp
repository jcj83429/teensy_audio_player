#include "ui.h"
#include <Arduino.h>

#define DEBOUNCE_TIME 20

struct KeyInfo keys[] = {
  [KEY_PLAY] = {
    .pin = PIN_KEY_PLAY,
    .event = KEY_EV_NONE,
    .lastState = 0,
    .lastChangeTime = 0,
    .lastEventTime = 0,
  },
  [KEY_PREV] = {
    .pin = PIN_KEY_PREV,
    .event = KEY_EV_NONE,
    .lastState = 0,
    .lastChangeTime = 0,
    .lastEventTime = 0,
  },
  [KEY_NEXT] = {
    .pin = PIN_KEY_NEXT,
    .event = KEY_EV_NONE,
    .lastState = 0,
    .lastChangeTime = 0,
    .lastEventTime = 0,
  },
};

void updateKeyStates(){
  unsigned long now = millis();
  for(int keyId = 0; keyId < NUM_KEYS; keyId++){
    bool newState = digitalRead(keys[keyId].pin);

    if(newState != keys[keyId].lastState){
      keys[keyId].lastState = newState;
      keys[keyId].lastChangeTime = now;
      keys[keyId].event = KEY_EV_NONE;
      continue;
    }

    if(keys[keyId].lastChangeTime - now > DEBOUNCE_TIME && keys[keyId].lastEventTime < keys[keyId].lastChangeTime){
      keys[keyId].event = newState ? KEY_EV_DOWN : KEY_EV_UP;
      keys[keyId].lastEventTime = now;
      Serial.print("KEY ");
      Serial.print(keyId);
      Serial.print(" EVENT ");
      Serial.println(keys[keyId].event);
    }else{
      keys[keyId].event = KEY_EV_NONE;
    }
  }
}
