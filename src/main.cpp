#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
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

constexpr uint8_t DS3231_I2C_ADDR     = 0x68;
constexpr uint32_t RTC_POLL_PERIOD_MS = 1000;

AppLogic::SystemState gState;
uint32_t gLastRtcPollMs = 0;
uint16_t gCachedRtcMin  = 600; // Fallback inicial 10:00

static uint8_t bcdToDec(uint8_t val) {
    return static_cast<uint8_t>((val >> 4) * 10U + (val & 0x0FU));
}

static uint16_t pollRtcMinutes(uint32_t nowMs) {
    if ((nowMs - gLastRtcPollMs) < RTC_POLL_PERIOD_MS) {
        return gCachedRtcMin;
    }
    gLastRtcPollMs = nowMs;

    Wire.beginTransmission(DS3231_I2C_ADDR);
    Wire.write(0x01); // Registro de minutos (0x01) y horas (0x02)
    if (Wire.endTransmission() == 0 && Wire.requestFrom(DS3231_I2C_ADDR, static_cast<uint8_t>(2)) == 2) {
        const uint8_t minutes = bcdToDec(Wire.read() & 0x7FU);
        const uint8_t hours   = bcdToDec(Wire.read() & 0x3FU);
        gCachedRtcMin = static_cast<uint16_t>((hours * 60U + minutes) % 1440U);
    } else {
        gCachedRtcMin = static_cast<uint16_t>((600UL + (nowMs / 60000UL)) % 1440UL);
    }
    return gCachedRtcMin;
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    pinMode(PIN_MAINS_SENSE, INPUT);
    pinMode(PIN_ACTUATOR, OUTPUT);
    AppLogic::initState(gState);
    Serial.println(F("[BOOT] ControladorPeceraDistancia - SLICE-02 Inicializado"));
}

void loop() {
    const uint32_t nowMs = millis();
    const bool mainsHigh = (digitalRead(PIN_MAINS_SENSE) == HIGH);
    const uint16_t currentRtcMin = pollRtcMinutes(nowMs);

    const AppLogic::MainsStatus prevMains = gState.mainsStatus;
    const AppLogic::LampTargetMode prevTarget = gState.targetLampMode;

    AppLogic::updateMainsPower(gState, mainsHigh, currentRtcMin, nowMs);
    AppLogic::evaluatePhotoperiod(gState, currentRtcMin);

    digitalWrite(PIN_ACTUATOR, gState.onBatteryBackup ? HIGH : LOW);

    if (gState.mainsStatus != prevMains) {
        if (gState.mainsStatus == AppLogic::MAINS_LOST) {
            Serial.println(F("[ENERGIA] Corte de 220V confirmado -> Respaldo bateria activo"));
        } else if (gState.mainsStatus == AppLogic::MAINS_RESTORING_WAIT) {
            Serial.println(F("[ENERGIA] Retorno de 220V confirmado -> Esperando 3000ms booteo lampara"));
        }
    }

    if (gState.targetLampMode != prevTarget) {
        if (gState.targetLampMode == AppLogic::LAMP_WHITE) {
            Serial.println(F("[FOTOPERIODO] Objetivo lampara -> LUZ BLANCA"));
        } else if (gState.targetLampMode == AppLogic::LAMP_OFF) {
            Serial.println(F("[FOTOPERIODO] Objetivo lampara -> APAGADA"));
        }
    }
}
#endif
