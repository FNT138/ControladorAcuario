#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace AppLogic {

// Constantes de tiempo y capacidad (aptas para 2KB SRAM de Arduino Nano)
static const uint32_t DEFAULT_MAINS_DEBOUNCE_MS     = 500;
static const uint32_t DEFAULT_LAMP_BOOT_SETTLE_MS   = 3000;
static const uint32_t DEFAULT_IR_INTER_CMD_DELAY_MS = 250;
static const uint8_t  OFFLINE_QUEUE_CAPACITY        = 16;

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

enum IrCommandType : uint8_t {
    IR_CMD_NONE = 0,
    IR_CMD_ON,
    IR_CMD_WHITE,
    IR_CMD_OFF
};

enum IrCloneSlot : uint8_t {
    CLONE_NONE = 0,
    CLONE_WAIT_ON,
    CLONE_WAIT_WHITE,
    CLONE_WAIT_OFF
};

enum EventType : uint8_t {
    EVT_NONE = 0,
    EVT_POWER_LOST,
    EVT_POWER_RESTORED,
    EVT_LAMP_WHITE_APPLIED,
    EVT_LAMP_OFF_APPLIED,
    EVT_SCHEDULE_ON,
    EVT_SCHEDULE_OFF,
    EVT_PHOTO_REQUESTED
};

struct IrCodeSet {
    uint32_t codeOn;
    uint32_t codeWhite;
    uint32_t codeOff;
};

struct OfflineEvent {
    EventType eventType;
    uint16_t timestampMinutes; // 0..1439 (HH*60 + MM del RTC DS3231)
    LampTargetMode lampMode;
    bool photoRequested;
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

    // SLICE-03: Secuenciador IR TX y Clonador IR RX
    IrCodeSet irCodes;
    IrCloneSlot cloneSlot;
    uint8_t irSeqStep;
    uint32_t irLastStepMs;
    uint32_t irInterCmdDelayMs;
    IrCommandType lastIrCommandSent;
    uint32_t lastIrRawCodeSent;

    // SLICE-04: Cola Circular Offline (Store-and-Forward) y Cámara/Telegram
    OfflineEvent eventQueue[OFFLINE_QUEUE_CAPACITY];
    uint8_t queueHead;
    uint8_t queueTail;
    uint8_t queueCount;
    uint8_t queueOverwrites;
    bool internetAvailable;
    bool localCameraTriggerPending;
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

// SLICE-03: Secuenciador Infrarrojo (IR TX) y Clonador (IR RX)
void requestLampTransition(SystemState& state, LampTargetMode desiredMode, uint32_t currentMs);
IrCommandType stepIrSequencer(SystemState& state, uint32_t currentMs, uint32_t* outRawCode = nullptr);
void startIrClone(SystemState& state, IrCloneSlot slot);
bool applyCapturedIrCode(SystemState& state, uint32_t capturedCode);

// SLICE-04: Cola Circular Offline Store-and-Forward y Comandos Remotos
bool pushOfflineEvent(SystemState& state, EventType type, uint16_t timestampMin, LampTargetMode mode, bool requestPhoto);
bool popOfflineEvent(SystemState& state, OfflineEvent& outEvent);
bool dispatchNextQueuedEvent(SystemState& state, bool internetReady, OfflineEvent& outDispatched);
bool processRemoteCommand(SystemState& state, const char* cmd, uint16_t currentRtcMin, uint32_t currentMs, char* replyBuf, size_t replyBufSize);

} // namespace AppLogic
