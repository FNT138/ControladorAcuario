#ifdef ARDUINO
#include <Arduino.h>
#include "controller.h"

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ARDUINO_ESP32C3_DEV)
constexpr uint8_t PIN_MAINS_SENSE = 3;  // ESP32-C3 SuperMini GPIO 3
constexpr uint8_t PIN_ACTUATOR    = 8;  // LED onboard ESP32-C3 SuperMini
#elif defined(ESP32)
constexpr uint8_t PIN_MAINS_SENSE = 34; // ESP32 clásico GPIO 34 (input-only)
constexpr uint8_t PIN_ACTUATOR    = 2;  // LED onboard ESP32 clásico
#else
constexpr uint8_t PIN_MAINS_SENSE = 2;  // Arduino Nano D2
constexpr uint8_t PIN_ACTUATOR    = 13; // LED onboard Arduino Nano D13
#endif

AppLogic::SystemState gState;

void setup() {
    Serial.begin(115200);
    pinMode(PIN_MAINS_SENSE, INPUT);
    pinMode(PIN_ACTUATOR, OUTPUT);
    AppLogic::initState(gState);
    Serial.println(F("[BOOT] ControladorPeceraDistancia - SLICE-01 Inicializado"));
}

void loop() {
    const uint32_t nowMs = millis();
    const bool mainsHigh = (digitalRead(PIN_MAINS_SENSE) == HIGH);
    const AppLogic::MainsStatus prevStatus = gState.mainsStatus;

    AppLogic::updateMainsPower(gState, mainsHigh, 0, nowMs);
    digitalWrite(PIN_ACTUATOR, gState.onBatteryBackup ? HIGH : LOW);

    if (gState.mainsStatus != prevStatus) {
        if (gState.mainsStatus == AppLogic::MAINS_LOST) {
            Serial.println(F("[ENERGIA] Corte de 220V confirmado -> Respaldo bateria activo"));
        } else if (gState.mainsStatus == AppLogic::MAINS_RESTORING_WAIT) {
            Serial.println(F("[ENERGIA] Retorno de 220V confirmado -> Esperando 3000ms booteo lampara"));
        } else if (gState.mainsStatus == AppLogic::MAINS_OK && gState.postRestoreActionPending) {
            Serial.println(F("[ENERGIA] Lampara estabilizada -> Accion post-corte pendiente"));
        }
    }
}
#endif
