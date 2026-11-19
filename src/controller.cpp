#include "controller.h"

namespace AppLogic {

void initState(SystemState& state) {
    state.sensorRawValue = 0;
    state.actuatorActive = false;
    state.lastUpdateMs = 0;

    state.mainsStatus = MAINS_OK;
    state.onBatteryBackup = false;
    state.postRestoreActionPending = false;
    state.mainsTransitionMs = 0;
    state.mainsDebounceMs = DEFAULT_MAINS_DEBOUNCE_MS;
    state.lampBootSettleMs = DEFAULT_LAMP_BOOT_SETTLE_MS;
}

bool evaluateThreshold(SystemState& state, uint16_t sensorReading, uint16_t threshold, uint32_t currentMs, uint32_t intervalMs) {
    if ((currentMs - state.lastUpdateMs) < intervalMs) {
        return state.actuatorActive;
    }
    state.lastUpdateMs = currentMs;
    state.sensorRawValue = sensorReading;
    state.actuatorActive = (sensorReading >= threshold);
    return state.actuatorActive;
}

void updateMainsPower(SystemState& state, bool mainsPinHigh, uint16_t currentRtcMin, uint32_t currentMs) {
    (void)currentRtcMin;

    switch (state.mainsStatus) {
        case MAINS_OK:
            if (!mainsPinHigh) {
                state.mainsStatus = MAINS_DEBOUNCE_LOST;
                state.mainsTransitionMs = currentMs;
            }
            break;

        case MAINS_DEBOUNCE_LOST:
            if (mainsPinHigh) {
                state.mainsStatus = MAINS_OK;
                state.mainsTransitionMs = currentMs;
            } else if ((currentMs - state.mainsTransitionMs) >= state.mainsDebounceMs) {
                state.mainsStatus = MAINS_LOST;
                state.onBatteryBackup = true;
                state.postRestoreActionPending = false;
                state.mainsTransitionMs = currentMs;
            }
            break;

        case MAINS_LOST:
            if (mainsPinHigh) {
                state.mainsStatus = MAINS_DEBOUNCE_RESTORED;
                state.mainsTransitionMs = currentMs;
            }
            break;

        case MAINS_DEBOUNCE_RESTORED:
            if (!mainsPinHigh) {
                state.mainsStatus = MAINS_LOST;
                state.mainsTransitionMs = currentMs;
            } else if ((currentMs - state.mainsTransitionMs) >= state.mainsDebounceMs) {
                state.mainsStatus = MAINS_RESTORING_WAIT;
                state.onBatteryBackup = false;
                state.mainsTransitionMs = currentMs;
            }
            break;

        case MAINS_RESTORING_WAIT:
            if (!mainsPinHigh) {
                state.mainsStatus = MAINS_DEBOUNCE_LOST;
                state.mainsTransitionMs = currentMs;
            } else if ((currentMs - state.mainsTransitionMs) >= state.lampBootSettleMs) {
                state.mainsStatus = MAINS_OK;
                state.postRestoreActionPending = true;
                state.mainsTransitionMs = currentMs;
            }
            break;
    }
}

} // namespace AppLogic
