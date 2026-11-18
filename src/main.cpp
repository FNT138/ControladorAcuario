#ifdef ARDUINO
#include <Arduino.h>
#include "controller.h"

constexpr uint8_t PIN_SENSOR   = A0;
constexpr uint8_t PIN_ACTUATOR = 13;
constexpr uint16_t THRESHOLD   = 512;
constexpr uint32_t INTERVAL_MS = 100;

AppLogic::SystemState gState;

void setup() {
    Serial.begin(115200);
    pinMode(PIN_SENSOR, INPUT);
    pinMode(PIN_ACTUATOR, OUTPUT);
    AppLogic::initState(gState);
    Serial.println(F("[BOOT] Sistema Arduino Inicializado"));
}

void loop() {
    uint16_t reading = analogRead(PIN_SENSOR);
    bool active = AppLogic::evaluateThreshold(gState, reading, THRESHOLD, millis(), INTERVAL_MS);
    digitalWrite(PIN_ACTUATOR, active ? HIGH : LOW);
}
#endif
