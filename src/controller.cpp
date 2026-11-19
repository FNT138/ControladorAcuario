#include "controller.h"
#include <string.h>

namespace AppLogic {

namespace {

void copyText(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }
    size_t i = 0;
    while (src[i] != '\0' && (i + 1) < dstSize) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

void appendText(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }
    size_t len = strlen(dst);
    size_t i = 0;
    while (src[i] != '\0' && (len + i + 1) < dstSize) {
        dst[len + i] = src[i];
        ++i;
    }
    dst[len + i] = '\0';
}

void appendUint(char* dst, size_t dstSize, uint16_t value) {
    char tmp[6];
    uint8_t pos = 0;
    if (value == 0) {
        tmp[pos++] = '0';
    } else {
        uint16_t v = value;
        char rev[6];
        uint8_t r = 0;
        while (v > 0 && r < 5) {
            rev[r++] = static_cast<char>('0' + (v % 10U));
            v /= 10U;
        }
        while (r > 0) {
            tmp[pos++] = rev[--r];
        }
    }
    tmp[pos] = '\0';
    appendText(dst, dstSize, tmp);
}

bool isDigitChar(char c) {
    return (c >= '0' && c <= '9');
}

bool parseHhMmToken(const char* s, uint16_t& outMinutes) {
    if (!isDigitChar(s[0]) || !isDigitChar(s[1]) || s[2] != ':' ||
        !isDigitChar(s[3]) || !isDigitChar(s[4])) {
        return false;
    }
    const uint16_t hh = static_cast<uint16_t>((s[0] - '0') * 10 + (s[1] - '0'));
    const uint16_t mm = static_cast<uint16_t>((s[3] - '0') * 10 + (s[4] - '0'));
    if (hh >= 24U || mm >= 60U) {
        return false;
    }
    outMinutes = static_cast<uint16_t>(hh * 60U + mm);
    return true;
}

bool parseUint16Token(const char* s, uint16_t& outValue) {
    while (*s == ' ') {
        ++s;
    }
    if (!isDigitChar(*s)) {
        return false;
    }
    uint32_t acc = 0;
    while (isDigitChar(*s)) {
        acc = acc * 10UL + static_cast<uint32_t>(*s - '0');
        if (acc > 65535UL) {
            return false;
        }
        ++s;
    }
    while (*s == ' ') {
        ++s;
    }
    if (*s != '\0') {
        return false;
    }
    outValue = static_cast<uint16_t>(acc);
    return true;
}

} // namespace

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

    // Códigos NEC universales del control IR de 24 teclas por defecto
    state.irCodes.codeOn = 0x00F7C03FUL;
    state.irCodes.codeWhite = 0x00F7E01FUL;
    state.irCodes.codeOff = 0x00F740BFUL;
    state.cloneSlot = CLONE_NONE;
    state.irSeqStep = 0;
    state.irLastStepMs = 0;
    state.irInterCmdDelayMs = DEFAULT_IR_INTER_CMD_DELAY_MS;
    state.lastIrCommandSent = IR_CMD_NONE;
    state.lastIrRawCodeSent = 0;

    for (uint8_t i = 0; i < OFFLINE_QUEUE_CAPACITY; ++i) {
        state.eventQueue[i].eventType = EVT_NONE;
        state.eventQueue[i].timestampMinutes = 0;
        state.eventQueue[i].lampMode = LAMP_MODE_UNKNOWN;
        state.eventQueue[i].photoRequested = false;
    }
    state.queueHead = 0;
    state.queueTail = 0;
    state.queueCount = 0;
    state.queueOverwrites = 0;
    state.internetAvailable = false;
    state.localCameraTriggerPending = false;

    state.feederStatus = FEEDER_IDLE;
    state.feederOutputActive = false;
    state.feederPulseMs = DEFAULT_FEEDER_PULSE_MS;
    state.feederStartMs = 0;
    state.feederCooldownMs = DEFAULT_FEEDER_COOLDOWN_MS;
    state.feedDispensedCount = 0;
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
                pushOfflineEvent(state, EVT_POWER_LOST, state.currentRtcMin, state.appliedLampMode, true);
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
                pushOfflineEvent(state, EVT_POWER_RESTORED, state.currentRtcMin, state.appliedLampMode, true);
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
            pushOfflineEvent(state, EVT_LAMP_WHITE_APPLIED, state.currentRtcMin, LAMP_WHITE, true);
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
            pushOfflineEvent(state, EVT_LAMP_OFF_APPLIED, state.currentRtcMin, LAMP_OFF, true);
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

bool pushOfflineEvent(SystemState& state, EventType type, uint16_t timestampMin, LampTargetMode mode, bool requestPhoto) {
    if (type == EVT_NONE) {
        return false;
    }

    state.eventQueue[state.queueTail].eventType = type;
    state.eventQueue[state.queueTail].timestampMinutes = timestampMin % 1440U;
    state.eventQueue[state.queueTail].lampMode = mode;
    state.eventQueue[state.queueTail].photoRequested = requestPhoto;

    state.queueTail = static_cast<uint8_t>((state.queueTail + 1U) % OFFLINE_QUEUE_CAPACITY);

    if (state.queueCount == OFFLINE_QUEUE_CAPACITY) {
        state.queueHead = static_cast<uint8_t>((state.queueHead + 1U) % OFFLINE_QUEUE_CAPACITY);
        if (state.queueOverwrites < 255U) {
            ++state.queueOverwrites;
        }
    } else {
        ++state.queueCount;
    }

    if (requestPhoto) {
        state.localCameraTriggerPending = true;
    }
    return true;
}

bool popOfflineEvent(SystemState& state, OfflineEvent& outEvent) {
    if (state.queueCount == 0) {
        return false;
    }

    outEvent = state.eventQueue[state.queueHead];
    state.queueHead = static_cast<uint8_t>((state.queueHead + 1U) % OFFLINE_QUEUE_CAPACITY);
    --state.queueCount;
    return true;
}

bool dispatchNextQueuedEvent(SystemState& state, bool internetReady, OfflineEvent& outDispatched) {
    state.internetAvailable = internetReady;
    if (!internetReady || state.queueCount == 0) {
        return false;
    }
    return popOfflineEvent(state, outDispatched);
}

bool processRemoteCommand(SystemState& state, const char* cmd, uint16_t currentRtcMin, uint32_t currentMs, char* replyBuf, size_t replyBufSize) {
    if (replyBuf != nullptr && replyBufSize > 0) {
        replyBuf[0] = '\0';
    }
    if (cmd == nullptr) {
        return false;
    }

    while (*cmd == ' ') {
        ++cmd;
    }

    state.currentRtcMin = currentRtcMin % 1440U;

    if (strncmp(cmd, "/horario ", 9) == 0) {
        const char* p = cmd + 9;
        while (*p == ' ') {
            ++p;
        }
        uint16_t startMin = 0;
        uint16_t endMin = 0;
        if (!parseHhMmToken(p, startMin) || p[5] != ' ') {
            copyText(replyBuf, replyBufSize, "ERR: Formato /horario HH:MM HH:MM");
            return false;
        }
        const char* p2 = p + 6;
        while (*p2 == ' ') {
            ++p2;
        }
        if (!parseHhMmToken(p2, endMin)) {
            copyText(replyBuf, replyBufSize, "ERR: Formato /horario HH:MM HH:MM");
            return false;
        }
        setPhotoperiodSchedule(state, startMin, endMin);
        copyText(replyBuf, replyBufSize, "OK: Horario actualizado");
        return true;
    }

    if (strncmp(cmd, "/calibrar_ms ", 13) == 0) {
        uint16_t pulseMs = 0;
        if (!parseUint16Token(cmd + 13, pulseMs) || !setFeederPulseDuration(state, pulseMs)) {
            copyText(replyBuf, replyBufSize, "ERR: Rango permitido 50..2000 ms");
            return false;
        }
        copyText(replyBuf, replyBufSize, "OK: Pulso alimentador calibrado");
        return true;
    }

    if (strcmp(cmd, "/alimentar") == 0) {
        if (!triggerEmergencyFeed(state, state.currentRtcMin, currentMs, false)) {
            copyText(replyBuf, replyBufSize, "ERR: Alimentador bloqueado (bateria/cooldown)");
            return false;
        }
        copyText(replyBuf, replyBufSize, "OK: Alimentando (~3 pellets)");
        return true;
    }

    if (strcmp(cmd, "/alimentar_forzar") == 0) {
        if (!triggerEmergencyFeed(state, state.currentRtcMin, currentMs, true)) {
            copyText(replyBuf, replyBufSize, "ERR: Alimentador bloqueado en bateria");
            return false;
        }
        copyText(replyBuf, replyBufSize, "OK: Alimentacion forzada (~3 pellets)");
        return true;
    }

    if (strcmp(cmd, "/luz_blanca") == 0) {
        state.controlPolicy = POLICY_MANUAL_OVERRIDE;
        requestLampTransition(state, LAMP_WHITE, currentMs);
        copyText(replyBuf, replyBufSize, "OK: Luz blanca (manual)");
        return true;
    }

    if (strcmp(cmd, "/luz_off") == 0) {
        state.controlPolicy = POLICY_MANUAL_OVERRIDE;
        requestLampTransition(state, LAMP_OFF, currentMs);
        copyText(replyBuf, replyBufSize, "OK: Luz apagada (manual)");
        return true;
    }

    if (strcmp(cmd, "/auto") == 0) {
        state.controlPolicy = POLICY_AUTO_RTC;
        state.scheduleInitialized = false;
        evaluatePhotoperiod(state, state.currentRtcMin);
        copyText(replyBuf, replyBufSize, "OK: Modo automatico RTC");
        return true;
    }

    if (strcmp(cmd, "/foto") == 0) {
        pushOfflineEvent(state, EVT_PHOTO_REQUESTED, state.currentRtcMin, state.appliedLampMode, true);
        state.localCameraTriggerPending = true;
        copyText(replyBuf, replyBufSize, "OK: Foto solicitada");
        return true;
    }

    if (strcmp(cmd, "/clonar_ir") == 0) {
        startIrClone(state, CLONE_WAIT_ON);
        copyText(replyBuf, replyBufSize, "OK: Clonado IR iniciado (ON)");
        return true;
    }

    if (strcmp(cmd, "/estado") == 0) {
        copyText(replyBuf, replyBufSize, "220V:");
        appendText(replyBuf, replyBufSize, state.onBatteryBackup ? "CORTE" : "OK");
        appendText(replyBuf, replyBufSize, " LUZ:");
        appendText(replyBuf, replyBufSize, (state.targetLampMode == LAMP_WHITE) ? "BLANCA" : "OFF");
        appendText(replyBuf, replyBufSize, " COLA:");
        appendUint(replyBuf, replyBufSize, state.queueCount);
        appendText(replyBuf, replyBufSize, " FEED_MS=");
        appendUint(replyBuf, replyBufSize, state.feederPulseMs);
        appendText(replyBuf, replyBufSize, " DOSIS=");
        appendUint(replyBuf, replyBufSize, state.feedDispensedCount);
        return true;
    }

    if (strcmp(cmd, "/cola") == 0) {
        copyText(replyBuf, replyBufSize, "PENDIENTES:");
        appendUint(replyBuf, replyBufSize, state.queueCount);
        appendText(replyBuf, replyBufSize, " OVERWRITES:");
        appendUint(replyBuf, replyBufSize, state.queueOverwrites);
        return true;
    }

    copyText(replyBuf, replyBufSize, "ERR: Comando desconocido");
    return false;
}

bool setFeederPulseDuration(SystemState& state, uint16_t pulseMs) {
    if (pulseMs < MIN_FEEDER_PULSE_MS || pulseMs > MAX_FEEDER_PULSE_MS) {
        return false;
    }
    state.feederPulseMs = pulseMs;
    return true;
}

bool triggerEmergencyFeed(SystemState& state, uint16_t currentRtcMin, uint32_t currentMs, bool forceIgnoreCooldown) {
    state.currentRtcMin = currentRtcMin % 1440U;

    if (state.onBatteryBackup || state.mainsStatus == MAINS_LOST) {
        state.feederOutputActive = false;
        return false;
    }

    if (state.feederStatus == FEEDER_DISPENSING) {
        return false;
    }

    if (state.feederStatus == FEEDER_COOLDOWN) {
        if ((currentMs - state.feederStartMs) >= state.feederCooldownMs) {
            state.feederStatus = FEEDER_IDLE;
        } else if (!forceIgnoreCooldown) {
            return false;
        }
    }

    state.feederStatus = FEEDER_DISPENSING;
    state.feederOutputActive = true;
    state.feederStartMs = currentMs;
    return true;
}

bool stepFeeder(SystemState& state, uint16_t currentRtcMin, uint32_t currentMs) {
    state.currentRtcMin = currentRtcMin % 1440U;

    if (state.onBatteryBackup || state.mainsStatus == MAINS_LOST) {
        state.feederOutputActive = false;
        state.feederStatus = FEEDER_IDLE;
        return false;
    }

    if (state.feederStatus == FEEDER_DISPENSING) {
        if ((currentMs - state.feederStartMs) < state.feederPulseMs) {
            state.feederOutputActive = true;
            return true;
        }
        state.feederOutputActive = false;
        state.feederStatus = FEEDER_COOLDOWN;
        state.feederStartMs = currentMs;
        if (state.feedDispensedCount < 255U) {
            ++state.feedDispensedCount;
        }
        pushOfflineEvent(state, EVT_FEED_DISPENSED, state.currentRtcMin, state.appliedLampMode, true);
        return false;
    }

    if (state.feederStatus == FEEDER_COOLDOWN) {
        state.feederOutputActive = false;
        if ((currentMs - state.feederStartMs) >= state.feederCooldownMs) {
            state.feederStatus = FEEDER_IDLE;
        }
        return false;
    }

    state.feederOutputActive = false;
    return false;
}

} // namespace AppLogic
