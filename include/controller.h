#pragma once
#include <stdint.h>
#include <stdbool.h>

namespace AppLogic {

// Constantes de tiempo para SLICE-01 (detección de 220V y estabilización de lámpara)
static const uint32_t DEFAULT_MAINS_DEBOUNCE_MS   = 500;
static const uint32_t DEFAULT_LAMP_BOOT_SETTLE_MS = 3000;

enum MainsStatus : uint8_t {
    MAINS_OK = 0,
    MAINS_DEBOUNCE_LOST,
    MAINS_LOST,
    MAINS_DEBOUNCE_RESTORED,
    MAINS_RESTORING_WAIT
};

enum LampTargetMode : uint8_t {
    LAMP_MODE_UNKNOWN = 0,
    LAMP_OFF,
    LAMP_WHITE
};

enum ControlPolicy : uint8_t {
    POLICY_AUTO_RTC = 0,
    POLICY_MANUAL_OVERRIDE
};

struct SystemState {
    // Base legacy (compatibilidad con tests iniciales)
    uint16_t sensorRawValue;
    bool actuatorActive;
    uint32_t lastUpdateMs;

    // SLICE-01: Energía 220V y estabilización post-corte
    MainsStatus mainsStatus;
    bool onBatteryBackup;
    bool postRestoreActionPending;
    uint32_t mainsTransitionMs;
    uint32_t mainsDebounceMs;
    uint32_t lampBootSettleMs;

    // SLICE-02: Fotoperíodo RTC (minutos desde medianoche 0..1439)
    uint16_t scheduleStartMin; // Default 600 (10:00)
    uint16_t scheduleEndMin;   // Default 1080 (18:00)
    uint16_t currentRtcMin;
    bool lastWithinSchedule;
    bool scheduleInitialized;
    ControlPolicy controlPolicy;
    LampTargetMode targetLampMode;
    LampTargetMode appliedLampMode;
};

// Base
void initState(SystemState& state);
bool evaluateThreshold(SystemState& state, uint16_t sensorReading, uint16_t threshold, uint32_t currentMs, uint32_t intervalMs);

// SLICE-01: Detección No Bloqueante de Corte/Retorno 220V
void updateMainsPower(SystemState& state, bool mainsPinHigh, uint16_t currentRtcMin, uint32_t currentMs);

// SLICE-02: Fotoperíodo Diario RTC y Decisión Post-Corte
bool isMinuteInWindow(uint16_t currentMin, uint16_t startMin, uint16_t endMin);
void setPhotoperiodSchedule(SystemState& state, uint16_t startMin, uint16_t endMin);
void evaluatePhotoperiod(SystemState& state, uint16_t currentRtcMin);

} // namespace AppLogic
