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

    state.scheduleStartMin = 600; // 10:00
    state.scheduleEndMin = 1080;  // 18:00
    state.currentRtcMin = 0;
    state.lastWithinSchedule = false;
    state.scheduleInitialized = false;
    state.controlPolicy = POLICY_AUTO_RTC;
    state.targetLampMode = LAMP_MODE_UNKNOWN;
    state.appliedLampMode = LAMP_MODE_UNKNOWN;
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
    state.currentRtcMin = currentRtcMin % 1440U;

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

bool isMinuteInWindow(uint16_t currentMin, uint16_t startMin, uint16_t endMin) {
    const uint16_t cur = currentMin % 1440U;
    const uint16_t start = startMin % 1440U;
    const uint16_t end = endMin % 1440U;

    if (start == end) {
        return false;
    }
    if (start < end) {
        return (cur >= start) && (cur < end);
    }
    // Ventana nocturna que cruza medianoche (ej. 20:00 a 06:00)
    return (cur >= start) || (cur < end);
}

void setPhotoperiodSchedule(SystemState& state, uint16_t startMin, uint16_t endMin) {
    state.scheduleStartMin = startMin % 1440U;
    state.scheduleEndMin = endMin % 1440U;
    state.scheduleInitialized = false;
}

void evaluatePhotoperiod(SystemState& state, uint16_t currentRtcMin) {
    state.currentRtcMin = currentRtcMin % 1440U;
    const bool within = isMinuteInWindow(state.currentRtcMin, state.scheduleStartMin, state.scheduleEndMin);

    if (state.postRestoreActionPending) {
        state.postRestoreActionPending = false;
        state.targetLampMode = within ? LAMP_WHITE : LAMP_OFF;
        state.lastWithinSchedule = within;
        state.scheduleInitialized = true;
        return;
    }

    if (state.mainsStatus == MAINS_OK && state.controlPolicy == POLICY_AUTO_RTC) {
        if (!state.scheduleInitialized || (within != state.lastWithinSchedule)) {
            state.targetLampMode = within ? LAMP_WHITE : LAMP_OFF;
            state.lastWithinSchedule = within;
            state.scheduleInitialized = true;
        }
    }
}

} // namespace AppLogic
