#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include "controller.h"

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ARDUINO_ESP32C3_DEV)
constexpr uint8_t PIN_MAINS_SENSE = 3;  // ESP32-C3 SuperMini GPIO 3
constexpr uint8_t PIN_IR_TX       = 4;  // ESP32-C3 SuperMini GPIO 4 (KY-005 + 2N2222)
constexpr uint8_t PIN_IR_RX       = 5;  // ESP32-C3 SuperMini GPIO 5 (KY-022 VS1838B)
constexpr uint8_t PIN_ACTUATOR    = 8;  // LED onboard ESP32-C3 SuperMini
#elif defined(ESP32)
constexpr uint8_t PIN_MAINS_SENSE = 34; // ESP32 clásico GPIO 34 (input-only)
constexpr uint8_t PIN_IR_TX       = 4;  // ESP32 clásico GPIO 4 (KY-005 + 2N2222)
constexpr uint8_t PIN_IR_RX       = 15; // ESP32 clásico GPIO 15 (KY-022 VS1838B)
constexpr uint8_t PIN_ACTUATOR    = 2;  // LED onboard ESP32 clásico
#else
constexpr uint8_t PIN_MAINS_SENSE = 2;  // Arduino Nano D2
constexpr uint8_t PIN_IR_TX       = 3;  // Arduino Nano D3 (KY-005 + 2N2222)
constexpr uint8_t PIN_IR_RX       = 4;  // Arduino Nano D4 (KY-022 VS1838B)
constexpr uint8_t PIN_ACTUATOR    = 13; // LED onboard Arduino Nano D13
#endif

constexpr uint8_t DS3231_I2C_ADDR          = 0x68;
constexpr uint32_t RTC_POLL_PERIOD_MS      = 1000;
constexpr uint32_t TELEMETRY_DISPATCH_MS   = 500;
constexpr size_t SERIAL_CMD_BUF_SIZE       = 48;
constexpr size_t SERIAL_REPLY_BUF_SIZE     = 80;

AppLogic::SystemState gState;
uint32_t gLastRtcPollMs      = 0;
uint32_t gLastDispatchMs     = 0;
uint16_t gCachedRtcMin       = 600; // Fallback inicial 10:00
bool gBridgeInternetReady    = true;

char gCmdBuffer[SERIAL_CMD_BUF_SIZE];
uint8_t gCmdLen = 0;

static uint8_t bcdToDec(uint8_t val) {
    return static_cast<uint8_t>((val >> 4) * 10U + (val & 0x0FU));
}

static uint16_t pollRtcMinutes(uint32_t nowMs) {
    if ((nowMs - gLastRtcPollMs) < RTC_POLL_PERIOD_MS) {
        return gCachedRtcMin;
    }
    gLastRtcPollMs = nowMs;

    Wire.beginTransmission(DS3231_I2C_ADDR);
    Wire.write(0x01);
    if (Wire.endTransmission() == 0 && Wire.requestFrom(DS3231_I2C_ADDR, static_cast<uint8_t>(2)) == 2) {
        const uint8_t minutes = bcdToDec(Wire.read() & 0x7FU);
        const uint8_t hours   = bcdToDec(Wire.read() & 0x3FU);
        gCachedRtcMin = static_cast<uint16_t>((hours * 60U + minutes) % 1440U);
    } else {
        gCachedRtcMin = static_cast<uint16_t>((600UL + (nowMs / 60000UL)) % 1440UL);
    }
    return gCachedRtcMin;
}

static void emitIrCarrierMark(uint16_t durationUs) {
    const uint16_t cycles = durationUs / 26U;
    for (uint16_t i = 0; i < cycles; ++i) {
        digitalWrite(PIN_IR_TX, HIGH);
        delayMicroseconds(9);
        digitalWrite(PIN_IR_TX, LOW);
        delayMicroseconds(17);
    }
}

static void emitIrSpace(uint16_t durationUs) {
    digitalWrite(PIN_IR_TX, LOW);
    delayMicroseconds(durationUs);
}

static void transmitNecIrFrame(uint32_t code) {
    emitIrCarrierMark(9000);
    emitIrSpace(4500);
    for (int8_t bit = 31; bit >= 0; --bit) {
        emitIrCarrierMark(560);
        if ((code >> bit) & 1UL) {
            emitIrSpace(1690);
        } else {
            emitIrSpace(560);
        }
    }
    emitIrCarrierMark(560);
    digitalWrite(PIN_IR_TX, LOW);
}

static void pollSerialCommands(uint16_t currentRtcMin, uint32_t nowMs) {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (gCmdLen > 0) {
                gCmdBuffer[gCmdLen] = '\0';
                if (strcmp(gCmdBuffer, "/net_on") == 0) {
                    gBridgeInternetReady = true;
                    Serial.println(F("[NET] Internet disponible -> Despachando cola FIFO"));
                } else if (strcmp(gCmdBuffer, "/net_off") == 0) {
                    gBridgeInternetReady = false;
                    Serial.println(F("[NET] Sin internet -> Modo Store-and-Forward activo"));
                } else {
                    char reply[SERIAL_REPLY_BUF_SIZE];
                    AppLogic::processRemoteCommand(gState, gCmdBuffer, currentRtcMin, nowMs, reply, sizeof(reply));
                    Serial.print(F("[TELEGRAM-REPLY] "));
                    Serial.println(reply);
                }
                gCmdLen = 0;
            }
        } else if ((gCmdLen + 1U) < SERIAL_CMD_BUF_SIZE) {
            gCmdBuffer[gCmdLen++] = c;
        }
    }
}

static void stepOfflineQueueDispatch(uint32_t nowMs) {
    if (gState.localCameraTriggerPending) {
        gState.localCameraTriggerPending = false;
        Serial.println(F("[CAM-TRIGGER] Capturando foto local en celular/ESP32-CAM"));
    }

    if ((nowMs - gLastDispatchMs) < TELEMETRY_DISPATCH_MS) {
        return;
    }
    gLastDispatchMs = nowMs;

    const bool internetOk = (!gState.onBatteryBackup) && gBridgeInternetReady;
    AppLogic::OfflineEvent ev;
    if (AppLogic::dispatchNextQueuedEvent(gState, internetOk, ev)) {
        Serial.print(F("[TELEGRAM-TX] Evento="));
        Serial.print(static_cast<uint8_t>(ev.eventType));
        Serial.print(F(" Min="));
        Serial.print(ev.timestampMinutes);
        Serial.print(F(" Luz="));
        Serial.print(static_cast<uint8_t>(ev.lampMode));
        Serial.print(F(" Foto="));
        Serial.println(ev.photoRequested ? 1 : 0);
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    pinMode(PIN_MAINS_SENSE, INPUT);
    pinMode(PIN_IR_TX, OUTPUT);
    digitalWrite(PIN_IR_TX, LOW);
    pinMode(PIN_IR_RX, INPUT);
    pinMode(PIN_ACTUATOR, OUTPUT);
    AppLogic::initState(gState);
    Serial.println(F("[BOOT] ControladorPeceraDistancia - SLICE-04 Inicializado"));
}

void loop() {
    const uint32_t nowMs = millis();
    const bool mainsHigh = (digitalRead(PIN_MAINS_SENSE) == HIGH);
    const uint16_t currentRtcMin = pollRtcMinutes(nowMs);

    AppLogic::updateMainsPower(gState, mainsHigh, currentRtcMin, nowMs);
    AppLogic::evaluatePhotoperiod(gState, currentRtcMin);
    pollSerialCommands(currentRtcMin, nowMs);

    uint32_t rawIrCode = 0;
    const AppLogic::IrCommandType irCmd = AppLogic::stepIrSequencer(gState, nowMs, &rawIrCode);
    if (irCmd != AppLogic::IR_CMD_NONE) {
        transmitNecIrFrame(rawIrCode);
        Serial.print(F("[IR-TX] Cmd="));
        Serial.print(static_cast<uint8_t>(irCmd));
        Serial.print(F(" Code=0x"));
        Serial.println(rawIrCode, HEX);
    }

    stepOfflineQueueDispatch(nowMs);
    digitalWrite(PIN_ACTUATOR, gState.onBatteryBackup ? HIGH : LOW);
}
#endif
