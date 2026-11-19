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

    state.irCodes.codeOn = 0x00FF02FDUL;
    state.irCodes.codeWhite = 0x00FF22DDUL;
    state.irCodes.codeOff = 0x00FF827DUL;
    state.cloneSlot = CLONE_NONE;
    state.irSeqStep = 0;
    state.irLastStepMs = 0;
    state.irInterCmdDelayMs = DEFAULT_IR_INTER_CMD_DELAY_MS;
    state.lastIrCommandSent = IR_CMD_NONE;
    state.lastIrRawCodeSent = 0;
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
        const LampTargetMode desired = within ? LAMP_WHITE : LAMP_OFF;
        requestLampTransition(state, desired, state.mainsTransitionMs);
        state.lastWithinSchedule = within;
        state.scheduleInitialized = true;
        return;
    }

    if (state.mainsStatus == MAINS_OK && state.controlPolicy == POLICY_AUTO_RTC) {
        if (!state.scheduleInitialized || (within != state.lastWithinSchedule)) {
            const LampTargetMode desired = within ? LAMP_WHITE : LAMP_OFF;
            requestLampTransition(state, desired, state.mainsTransitionMs);
            state.lastWithinSchedule = within;
            state.scheduleInitialized = true;
        }
    }
}

void requestLampTransition(SystemState& state, LampTargetMode desiredMode, uint32_t currentMs) {
    state.targetLampMode = desiredMode;
    if (desiredMode == LAMP_WHITE || desiredMode == LAMP_OFF) {
        state.irSeqStep = 1;
        state.irLastStepMs = currentMs;
    } else {
        state.irSeqStep = 0;
    }
}

IrCommandType stepIrSequencer(SystemState& state, uint32_t currentMs, uint32_t* outRawCode) {
    if (outRawCode != nullptr) {
        *outRawCode = 0;
    }

    if (state.irSeqStep == 0) {
        return IR_CMD_NONE;
    }

    IrCommandType cmd = IR_CMD_NONE;
    uint32_t rawCode = 0;

    if (state.targetLampMode == LAMP_WHITE) {
        if (state.irSeqStep == 1) {
            cmd = IR_CMD_ON;
            rawCode = state.irCodes.codeOn;
            state.irLastStepMs = currentMs;
            state.irSeqStep = 2;
        } else if (state.irSeqStep == 2) {
            if ((currentMs - state.irLastStepMs) < state.irInterCmdDelayMs) {
                return IR_CMD_NONE;
            }
            cmd = IR_CMD_WHITE;
            rawCode = state.irCodes.codeWhite;
            state.irLastStepMs = currentMs;
            state.irSeqStep = 3;
        } else if (state.irSeqStep == 3) {
            if ((currentMs - state.irLastStepMs) < state.irInterCmdDelayMs) {
                return IR_CMD_NONE;
            }
            cmd = IR_CMD_WHITE;
            rawCode = state.irCodes.codeWhite;
            state.irLastStepMs = currentMs;
            state.irSeqStep = 0;
            state.appliedLampMode = LAMP_WHITE;
        } else {
            state.irSeqStep = 0;
            return IR_CMD_NONE;
        }
    } else if (state.targetLampMode == LAMP_OFF) {
        if (state.irSeqStep == 1) {
            cmd = IR_CMD_OFF;
            rawCode = state.irCodes.codeOff;
            state.irLastStepMs = currentMs;
            state.irSeqStep = 2;
        } else if (state.irSeqStep == 2) {
            if ((currentMs - state.irLastStepMs) < state.irInterCmdDelayMs) {
                return IR_CMD_NONE;
            }
            cmd = IR_CMD_OFF;
            rawCode = state.irCodes.codeOff;
            state.irLastStepMs = currentMs;
            state.irSeqStep = 0;
            state.appliedLampMode = LAMP_OFF;
        } else {
            state.irSeqStep = 0;
            return IR_CMD_NONE;
        }
    } else {
        state.irSeqStep = 0;
        return IR_CMD_NONE;
    }

    state.lastIrCommandSent = cmd;
    state.lastIrRawCodeSent = rawCode;
    if (outRawCode != nullptr) {
        *outRawCode = rawCode;
    }
    return cmd;
}

void startIrClone(SystemState& state, IrCloneSlot slot) {
    state.cloneSlot = slot;
}

bool applyCapturedIrCode(SystemState& state, uint32_t capturedCode) {
    if (state.cloneSlot == CLONE_NONE || capturedCode == 0UL || capturedCode == 0xFFFFFFFFUL) {
        return false;
    }

    switch (state.cloneSlot) {
        case CLONE_WAIT_ON:
            state.irCodes.codeOn = capturedCode;
            state.cloneSlot = CLONE_WAIT_WHITE;
            return true;
        case CLONE_WAIT_WHITE:
            state.irCodes.codeWhite = capturedCode;
            state.cloneSlot = CLONE_WAIT_OFF;
            return true;
        case CLONE_WAIT_OFF:
            state.irCodes.codeOff = capturedCode;
            state.cloneSlot = CLONE_NONE;
            return true;
        default:
            state.cloneSlot = CLONE_NONE;
            return false;
    }
}

} // namespace AppLogic
