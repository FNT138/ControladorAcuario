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
};

// Base
void initState(SystemState& state);
bool evaluateThreshold(SystemState& state, uint16_t sensorReading, uint16_t threshold, uint32_t currentMs, uint32_t intervalMs);

// SLICE-01: Detección No Bloqueante de Corte/Retorno 220V
void updateMainsPower(SystemState& state, bool mainsPinHigh, uint16_t currentRtcMin, uint32_t currentMs);

} // namespace AppLogic
