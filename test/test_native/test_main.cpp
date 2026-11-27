#include <unity.h>
#include <string.h>
#include "controller.h"

// Incluimos la implementación pura para entorno nativo de tests
#include "../src/controller.cpp"

void setUp(void) {}
void tearDown(void) {}

void test_initial_state_is_clean(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    TEST_ASSERT_EQUAL_UINT16(0, st.sensorRawValue);
    TEST_ASSERT_FALSE(st.actuatorActive);
    TEST_ASSERT_EQUAL_UINT32(0, st.lastUpdateMs);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_OK, st.mainsStatus);
    TEST_ASSERT_FALSE(st.onBatteryBackup);
    TEST_ASSERT_FALSE(st.postRestoreActionPending);
}

void test_threshold_activation_after_interval(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);

    // Antes de cumplirse el intervalo (50ms < 100ms), no debe cambiar el estado
    bool active = AppLogic::evaluateThreshold(st, 600, 512, 50, 100);
    TEST_ASSERT_FALSE(active);

    // Al cumplirse el intervalo (100ms >= 100ms) y superar umbral (600 >= 512), activa
    active = AppLogic::evaluateThreshold(st, 600, 512, 100, 100);
    TEST_ASSERT_TRUE(active);
    TEST_ASSERT_EQUAL_UINT16(600, st.sensorRawValue);
}

void test_slice01_ignore_glitch_under_500ms(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);

    // Cae 220V en t=1000ms -> entra en debounce
    AppLogic::updateMainsPower(st, false, 720, 1000);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_DEBOUNCE_LOST, st.mainsStatus);
    TEST_ASSERT_FALSE(st.onBatteryBackup);

    // A los 300ms (< 500ms) sigue en debounce
    AppLogic::updateMainsPower(st, false, 720, 1300);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_DEBOUNCE_LOST, st.mainsStatus);
    TEST_ASSERT_FALSE(st.onBatteryBackup);

    // Vuelve 220V en t=1400ms (microcorte de 400ms < 500ms) -> vuelve a MAINS_OK sin activar batería
    AppLogic::updateMainsPower(st, true, 720, 1400);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_OK, st.mainsStatus);
    TEST_ASSERT_FALSE(st.onBatteryBackup);
    TEST_ASSERT_FALSE(st.postRestoreActionPending);
}

void test_slice01_detect_power_loss_and_restore_after_3000ms(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);

    // 1. Cae 220V en t=1000ms y se mantiene LOW por 500ms (t=1500ms) -> MAINS_LOST
    AppLogic::updateMainsPower(st, false, 840, 1000);
    AppLogic::updateMainsPower(st, false, 840, 1500);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_LOST, st.mainsStatus);
    TEST_ASSERT_TRUE(st.onBatteryBackup);
    TEST_ASSERT_FALSE(st.postRestoreActionPending);

    // 2. Vuelve 220V en t=5000ms -> entra en MAINS_DEBOUNCE_RESTORED
    AppLogic::updateMainsPower(st, true, 845, 5000);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_DEBOUNCE_RESTORED, st.mainsStatus);
    TEST_ASSERT_TRUE(st.onBatteryBackup);

    // 3. Cumple 500ms estables en HIGH (t=5500ms) -> pasa a MAINS_RESTORING_WAIT (esperando 3000ms de booteo de lámpara)
    AppLogic::updateMainsPower(st, true, 845, 5500);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_RESTORING_WAIT, st.mainsStatus);
    TEST_ASSERT_FALSE(st.onBatteryBackup);
    TEST_ASSERT_FALSE(st.postRestoreActionPending);

    // 4. A los 2900ms de espera (t=8400ms < 5500+3000) todavía espera estabilización de la lámpara LED
    AppLogic::updateMainsPower(st, true, 845, 8400);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_RESTORING_WAIT, st.mainsStatus);
    TEST_ASSERT_FALSE(st.postRestoreActionPending);

    // 5. Al cumplirse los 3000ms (t=8500ms) pasa a MAINS_OK y dispara postRestoreActionPending = true
    AppLogic::updateMainsPower(st, true, 845, 8500);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_OK, st.mainsStatus);
    TEST_ASSERT_TRUE(st.postRestoreActionPending);
}

void test_slice02_photoperiod_window_day_and_overnight(void) {
    // Ventana diurna normal: 10:00 (600) a 18:00 (1080)
    TEST_ASSERT_FALSE(AppLogic::isMinuteInWindow(599, 600, 1080));
    TEST_ASSERT_TRUE(AppLogic::isMinuteInWindow(600, 600, 1080));
    TEST_ASSERT_TRUE(AppLogic::isMinuteInWindow(870, 600, 1080));
    TEST_ASSERT_FALSE(AppLogic::isMinuteInWindow(1080, 600, 1080));

    // Ventana nocturna cruzando medianoche: 20:00 (1200) a 06:00 (360)
    TEST_ASSERT_TRUE(AppLogic::isMinuteInWindow(1320, 1200, 360));
    TEST_ASSERT_TRUE(AppLogic::isMinuteInWindow(120, 1200, 360));
    TEST_ASSERT_FALSE(AppLogic::isMinuteInWindow(720, 1200, 360));
}

void test_slice02_post_restore_daytime_and_nighttime_decisions(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    AppLogic::setPhotoperiodSchedule(st, 600, 1080); // 10:00 a 18:00

    // Caso A: Vuelve la luz de un corte a las 14:30 (870 min -> dentro del fotoperíodo)
    st.postRestoreActionPending = true;
    AppLogic::evaluatePhotoperiod(st, 870);
    TEST_ASSERT_FALSE(st.postRestoreActionPending);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_WHITE, st.targetLampMode);

    // Caso B: Vuelve la luz de un corte a las 03:00 AM (180 min -> fuera del fotoperíodo)
    st.postRestoreActionPending = true;
    AppLogic::evaluatePhotoperiod(st, 180);
    TEST_ASSERT_FALSE(st.postRestoreActionPending);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_OFF, st.targetLampMode);
}

void test_slice02_daily_schedule_transitions(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    AppLogic::setPhotoperiodSchedule(st, 600, 1080); // 10:00 a 18:00

    // Arranca a las 09:59 (599) -> fuera de horario (LAMP_OFF)
    AppLogic::evaluatePhotoperiod(st, 599);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_OFF, st.targetLampMode);

    // Cruza a las 10:00 (600) -> enciende en blanco (LAMP_WHITE)
    AppLogic::evaluatePhotoperiod(st, 600);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_WHITE, st.targetLampMode);

    // A las 17:59 (1079) -> sigue en blanco (LAMP_WHITE)
    AppLogic::evaluatePhotoperiod(st, 1079);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_WHITE, st.targetLampMode);

    // Cruza a las 18:00 (1080) -> apaga la lámpara (LAMP_OFF)
    AppLogic::evaluatePhotoperiod(st, 1080);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_OFF, st.targetLampMode);
}

void test_slice03_ir_sequencer_lamp_white_3_steps(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    st.irCodes.codeOn    = 0x00FF02FDUL;
    st.irCodes.codeWhite = 0x00FF22DDUL;
    st.irCodes.codeOff   = 0x00FF827DUL;

    uint32_t rawCode = 0;
    AppLogic::requestLampTransition(st, AppLogic::LAMP_WHITE, 1000);

    // Paso 1 en t=1000ms: debe emitir IR_CMD_ON
    AppLogic::IrCommandType cmd = AppLogic::stepIrSequencer(st, 1000, &rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::IR_CMD_ON, cmd);
    TEST_ASSERT_EQUAL_HEX32(0x00FF02FDUL, rawCode);

    // Antes de 250ms (t=1100ms): no debe emitir nada
    cmd = AppLogic::stepIrSequencer(st, 1100, &rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::IR_CMD_NONE, cmd);

    // Paso 2 en t=1250ms: debe emitir IR_CMD_WHITE
    cmd = AppLogic::stepIrSequencer(st, 1250, &rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::IR_CMD_WHITE, cmd);
    TEST_ASSERT_EQUAL_HEX32(0x00FF22DDUL, rawCode);

    // Paso 3 en t=1500ms: re-emite IR_CMD_WHITE de refuerzo y marca appliedLampMode = LAMP_WHITE
    cmd = AppLogic::stepIrSequencer(st, 1500, &rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::IR_CMD_WHITE, cmd);
    TEST_ASSERT_EQUAL_HEX32(0x00FF22DDUL, rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_WHITE, st.appliedLampMode);

    // En t=1800ms ya finalizó la secuencia
    cmd = AppLogic::stepIrSequencer(st, 1800, &rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::IR_CMD_NONE, cmd);
}

void test_slice03_ir_sequencer_lamp_off_2_steps(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    st.irCodes.codeOff = 0x00FF827DUL;

    uint32_t rawCode = 0;
    AppLogic::requestLampTransition(st, AppLogic::LAMP_OFF, 2000);

    // Paso 1 en t=2000ms: emite IR_CMD_OFF
    AppLogic::IrCommandType cmd = AppLogic::stepIrSequencer(st, 2000, &rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::IR_CMD_OFF, cmd);
    TEST_ASSERT_EQUAL_HEX32(0x00FF827DUL, rawCode);

    // En t=2100ms (< 2250ms): espera no bloqueante
    cmd = AppLogic::stepIrSequencer(st, 2100, &rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::IR_CMD_NONE, cmd);

    // Paso 2 en t=2250ms: emite segundo IR_CMD_OFF de refuerzo y marca appliedLampMode = LAMP_OFF
    cmd = AppLogic::stepIrSequencer(st, 2250, &rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::IR_CMD_OFF, cmd);
    TEST_ASSERT_EQUAL_HEX32(0x00FF827DUL, rawCode);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_OFF, st.appliedLampMode);
}

void test_slice03_ir_clone_3_slots(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);

    // Si no está en modo clonado o el código es 0, rechaza
    TEST_ASSERT_FALSE(AppLogic::applyCapturedIrCode(st, 0x12345678UL));

    // Clonar botón ON
    AppLogic::startIrClone(st, AppLogic::CLONE_WAIT_ON);
    TEST_ASSERT_TRUE(AppLogic::applyCapturedIrCode(st, 0x11111111UL));
    TEST_ASSERT_EQUAL_HEX32(0x11111111UL, st.irCodes.codeOn);

    // Clonar botón WHITE
    AppLogic::startIrClone(st, AppLogic::CLONE_WAIT_WHITE);
    TEST_ASSERT_TRUE(AppLogic::applyCapturedIrCode(st, 0x22222222UL));
    TEST_ASSERT_EQUAL_HEX32(0x22222222UL, st.irCodes.codeWhite);

    // Clonar botón OFF
    AppLogic::startIrClone(st, AppLogic::CLONE_WAIT_OFF);
    TEST_ASSERT_TRUE(AppLogic::applyCapturedIrCode(st, 0x33333333UL));
    TEST_ASSERT_EQUAL_HEX32(0x33333333UL, st.irCodes.codeOff);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::CLONE_NONE, st.cloneSlot);
}

void test_slice04_offline_queue_store_and_forward_blackout_flow(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    AppLogic::setPhotoperiodSchedule(st, 600, 1080); // 10:00 a 18:00
    st.currentRtcMin = 135; // 02:15 AM

    // 1. Se corta la luz a las 02:15 AM (135 min) -> cae Wi-Fi de casa
    AppLogic::updateMainsPower(st, false, 135, 1000);
    AppLogic::updateMainsPower(st, false, 135, 1500);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::MAINS_LOST, st.mainsStatus);
    TEST_ASSERT_EQUAL_UINT8(1, st.queueCount);

    // 2. Vuelve la luz a las 03:40 AM (220 min), pero el router Wi-Fi aún no levantó (internetReady = false)
    st.currentRtcMin = 220;
    AppLogic::updateMainsPower(st, true, 220, 10000);
    AppLogic::updateMainsPower(st, true, 220, 10500); // MAINS_RESTORING_WAIT -> encola EVT_POWER_RESTORED
    AppLogic::updateMainsPower(st, true, 220, 13500); // 3000ms cumplidos -> postRestoreActionPending = true
    TEST_ASSERT_TRUE(st.postRestoreActionPending);
    TEST_ASSERT_EQUAL_UINT8(2, st.queueCount);

    // 3. El controlador evalúa el fotoperíodo a las 03:40 AM -> decide LAMP_OFF y ejecuta la secuencia IR
    AppLogic::evaluatePhotoperiod(st, 220);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_OFF, st.targetLampMode);
    AppLogic::requestLampTransition(st, st.targetLampMode, 13500);
    AppLogic::stepIrSequencer(st, 13500, nullptr);
    AppLogic::stepIrSequencer(st, 13750, nullptr); // Finaliza IR_CMD_OFF -> encola EVT_LAMP_OFF_APPLIED
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_OFF, st.appliedLampMode);
    TEST_ASSERT_EQUAL_UINT8(3, st.queueCount);

    // 4. Mientras no hay internet (internetReady = false), la cola NO se vacía
    AppLogic::OfflineEvent ev;
    TEST_ASSERT_FALSE(AppLogic::dispatchNextQueuedEvent(st, false, ev));
    TEST_ASSERT_EQUAL_UINT8(3, st.queueCount);

    // 5. Vuelve el Wi-Fi doméstico (internetReady = true) -> se despachan en orden FIFO exacto hacia Telegram
    TEST_ASSERT_TRUE(AppLogic::dispatchNextQueuedEvent(st, true, ev));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::EVT_POWER_LOST, ev.eventType);
    TEST_ASSERT_EQUAL_UINT16(135, ev.timestampMinutes);

    TEST_ASSERT_TRUE(AppLogic::dispatchNextQueuedEvent(st, true, ev));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::EVT_POWER_RESTORED, ev.eventType);
    TEST_ASSERT_EQUAL_UINT16(220, ev.timestampMinutes);

    TEST_ASSERT_TRUE(AppLogic::dispatchNextQueuedEvent(st, true, ev));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::EVT_LAMP_OFF_APPLIED, ev.eventType);
    TEST_ASSERT_EQUAL_UINT16(220, ev.timestampMinutes);

    TEST_ASSERT_EQUAL_UINT8(0, st.queueCount);
    TEST_ASSERT_FALSE(AppLogic::dispatchNextQueuedEvent(st, true, ev));
}

void test_slice04_ring_buffer_overflow_overwrites_oldest(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);

    // Insertar 18 eventos en cola de capacidad 16
    for (uint16_t i = 0; i < 18; ++i) {
        AppLogic::pushOfflineEvent(st, AppLogic::EVT_PHOTO_REQUESTED, static_cast<uint16_t>(100 + i), AppLogic::LAMP_WHITE, true);
    }

    TEST_ASSERT_EQUAL_UINT8(AppLogic::OFFLINE_QUEUE_CAPACITY, st.queueCount);
    TEST_ASSERT_EQUAL_UINT8(2, st.queueOverwrites);

    // El evento más antiguo conservado debe ser el #2 (timestamp 102)
    AppLogic::OfflineEvent ev;
    TEST_ASSERT_TRUE(AppLogic::popOfflineEvent(st, ev));
    TEST_ASSERT_EQUAL_UINT16(102, ev.timestampMinutes);
}

void test_slice04_remote_commands_parser(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    char reply[96];

    // /horario 09:30 19:00 -> 570 a 1140
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/horario 09:30 19:00", 600, 1000, reply, sizeof(reply)));
    TEST_ASSERT_EQUAL_UINT16(570, st.scheduleStartMin);
    TEST_ASSERT_EQUAL_UINT16(1140, st.scheduleEndMin);

    // /luz_blanca -> POLICY_MANUAL_OVERRIDE + LAMP_WHITE
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/luz_blanca", 200, 2000, reply, sizeof(reply)));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::POLICY_MANUAL_OVERRIDE, st.controlPolicy);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_WHITE, st.targetLampMode);

    // /luz_off -> POLICY_MANUAL_OVERRIDE + LAMP_OFF
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/luz_off", 700, 3000, reply, sizeof(reply)));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::POLICY_MANUAL_OVERRIDE, st.controlPolicy);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_OFF, st.targetLampMode);

    // /auto -> vuelve a POLICY_AUTO_RTC
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/auto", 700, 4000, reply, sizeof(reply)));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::POLICY_AUTO_RTC, st.controlPolicy);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::LAMP_WHITE, st.targetLampMode);

    // /foto -> encola EVT_PHOTO_REQUESTED y activa localCameraTriggerPending
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/foto", 705, 5000, reply, sizeof(reply)));
    TEST_ASSERT_TRUE(st.localCameraTriggerPending);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(1, st.queueCount);

    // /clonar_ir -> inicia CLONE_WAIT_ON
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/clonar_ir", 705, 6000, reply, sizeof(reply)));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::CLONE_WAIT_ON, st.cloneSlot);

    // /estado y /cola -> devuelven true y texto no vacío
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/estado", 705, 7000, reply, sizeof(reply)));
    TEST_ASSERT_GREATER_THAN_UINT32(0, static_cast<uint32_t>(strlen(reply)));
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/cola", 705, 7500, reply, sizeof(reply)));
    TEST_ASSERT_GREATER_THAN_UINT32(0, static_cast<uint32_t>(strlen(reply)));
}

void test_slice05_feeder_pulse_duration_bounds(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);

    // Default debe ser 350ms y códigos de control 24 teclas pre-cargados
    TEST_ASSERT_EQUAL_UINT16(AppLogic::DEFAULT_FEEDER_PULSE_MS, st.feederPulseMs);
    TEST_ASSERT_EQUAL_HEX32(0x00F7C03FUL, st.irCodes.codeOn);
    TEST_ASSERT_EQUAL_HEX32(0x00F7E01FUL, st.irCodes.codeWhite);
    TEST_ASSERT_EQUAL_HEX32(0x00F740BFUL, st.irCodes.codeOff);

    // Rechaza valores peligrosos (< 50ms o > 2000ms)
    TEST_ASSERT_FALSE(AppLogic::setFeederPulseDuration(st, 20));
    TEST_ASSERT_FALSE(AppLogic::setFeederPulseDuration(st, 2500));
    TEST_ASSERT_EQUAL_UINT16(AppLogic::DEFAULT_FEEDER_PULSE_MS, st.feederPulseMs);

    // Acepta valor calibrado válido (ej. 420ms para ~3 pellets)
    TEST_ASSERT_TRUE(AppLogic::setFeederPulseDuration(st, 420));
    TEST_ASSERT_EQUAL_UINT16(420, st.feederPulseMs);
}

void test_slice05_feeder_dispense_and_cooldown_and_battery_lock(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    AppLogic::setFeederPulseDuration(st, 350);

    // 1. Si está en corte de luz (onBatteryBackup = true), rechaza accionar el motor para cuidar la celda UR18650ZT
    st.onBatteryBackup = true;
    TEST_ASSERT_FALSE(AppLogic::triggerEmergencyFeed(st, 750, 1000, false));
    TEST_ASSERT_FALSE(st.feederOutputActive);
    st.onBatteryBackup = false;

    // 2. Con 220V OK, dispara una dosis en t=1000ms
    TEST_ASSERT_TRUE(AppLogic::triggerEmergencyFeed(st, 750, 1000, false));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::FEEDER_DISPENSING, st.feederStatus);
    TEST_ASSERT_TRUE(AppLogic::stepFeeder(st, 750, 1100)); // Sigue activo a los 100ms (< 350ms)
    TEST_ASSERT_TRUE(st.feederOutputActive);

    // 3. Al cumplirse los 350ms (t=1350ms), corta el motor, entra en COOLDOWN, suma 1 dosis y encola EVT_FEED_DISPENSED con foto
    TEST_ASSERT_FALSE(AppLogic::stepFeeder(st, 750, 1350));
    TEST_ASSERT_FALSE(st.feederOutputActive);
    TEST_ASSERT_EQUAL_UINT8(AppLogic::FEEDER_COOLDOWN, st.feederStatus);
    TEST_ASSERT_EQUAL_UINT8(1, st.feedDispensedCount);
    TEST_ASSERT_TRUE(st.localCameraTriggerPending);
    TEST_ASSERT_EQUAL_UINT8(1, st.queueCount);

    AppLogic::OfflineEvent ev;
    TEST_ASSERT_TRUE(AppLogic::popOfflineEvent(st, ev));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::EVT_FEED_DISPENSED, ev.eventType);
    TEST_ASSERT_TRUE(ev.photoRequested);

    // 4. Si intenta disparar otra vez durante el cooldown (t=5000ms < 1350+60000ms) con force=false, lo bloquea
    TEST_ASSERT_FALSE(AppLogic::triggerEmergencyFeed(st, 750, 5000, false));

    // 5. Con forceIgnoreCooldown=true (calibración en casa), sí permite disparar durante el cooldown
    TEST_ASSERT_TRUE(AppLogic::triggerEmergencyFeed(st, 750, 6000, true));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::FEEDER_DISPENSING, st.feederStatus);
}

void test_slice06_feeder_remote_commands(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    char reply[96];

    // 1. /calibrar_ms 420 -> actualiza feederPulseMs a 420
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/calibrar_ms 420", 750, 1000, reply, sizeof(reply)));
    TEST_ASSERT_EQUAL_UINT16(420, st.feederPulseMs);

    // 2. /calibrar_ms 5000 -> rechaza por exceder tope de seguridad (2000ms)
    TEST_ASSERT_FALSE(AppLogic::processRemoteCommand(st, "/calibrar_ms 5000", 750, 1000, reply, sizeof(reply)));
    TEST_ASSERT_EQUAL_UINT16(420, st.feederPulseMs);

    // 3. /alimentar en corte de luz (onBatteryBackup = true) -> rechaza para cuidar la celda UR18650ZT
    st.onBatteryBackup = true;
    TEST_ASSERT_FALSE(AppLogic::processRemoteCommand(st, "/alimentar", 750, 1000, reply, sizeof(reply)));
    TEST_ASSERT_FALSE(st.feederOutputActive);
    st.onBatteryBackup = false;

    // 4. /alimentar con 220V OK -> inicia pulso de 420ms
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/alimentar", 750, 1000, reply, sizeof(reply)));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::FEEDER_DISPENSING, st.feederStatus);
    TEST_ASSERT_TRUE(st.feederOutputActive);

    // Completa el pulso en t=1420ms -> pasa a COOLDOWN y suma 1 dosis
    TEST_ASSERT_FALSE(AppLogic::stepFeeder(st, 750, 1420));
    TEST_ASSERT_EQUAL_UINT8(1, st.feedDispensedCount);

    // 5. Segundo /alimentar inmediato durante cooldown (t=2000ms) -> bloqueado por seguridad
    TEST_ASSERT_FALSE(AppLogic::processRemoteCommand(st, "/alimentar", 750, 2000, reply, sizeof(reply)));

    // 6. /alimentar_forzar durante cooldown -> permite disparar para calibración en casa
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/alimentar_forzar", 750, 2500, reply, sizeof(reply)));
    TEST_ASSERT_EQUAL_UINT8(AppLogic::FEEDER_DISPENSING, st.feederStatus);

    // 7. /estado -> reporta DOSIS= y FEED_MS=
    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/estado", 750, 3000, reply, sizeof(reply)));
    TEST_ASSERT_NOT_NULL(strstr(reply, "DOSIS="));
}

void test_slice07_battery_percentage_and_commands(void) {
    AppLogic::SystemState st;
    AppLogic::initState(st);
    char reply[160];

    // Verificación de extremos y punto medio
    TEST_ASSERT_EQUAL_UINT8(0, AppLogic::computeBatteryPercent(2800));
    TEST_ASSERT_EQUAL_UINT8(0, AppLogic::computeBatteryPercent(3000));
    TEST_ASSERT_EQUAL_UINT8(50, AppLogic::computeBatteryPercent(3600));
    TEST_ASSERT_EQUAL_UINT8(100, AppLogic::computeBatteryPercent(4200));
    TEST_ASSERT_EQUAL_UINT8(100, AppLogic::computeBatteryPercent(4350));

    // Actualización de lectura y comandos /bateria y /estado
    AppLogic::updateBatteryReading(st, 4080); // (4080-3000)/1200 = 90%
    TEST_ASSERT_EQUAL_UINT16(4080, st.batteryMv);
    TEST_ASSERT_EQUAL_UINT8(90, st.batteryPercent);

    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/bateria", 750, 1000, reply, sizeof(reply)));
    TEST_ASSERT_NOT_NULL(strstr(reply, "4.08V"));
    TEST_ASSERT_NOT_NULL(strstr(reply, "90%"));

    TEST_ASSERT_TRUE(AppLogic::processRemoteCommand(st, "/estado", 750, 1000, reply, sizeof(reply)));
    TEST_ASSERT_NOT_NULL(strstr(reply, "BAT:90%"));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_clean);
    RUN_TEST(test_threshold_activation_after_interval);
    RUN_TEST(test_slice01_ignore_glitch_under_500ms);
    RUN_TEST(test_slice01_detect_power_loss_and_restore_after_3000ms);
    RUN_TEST(test_slice02_photoperiod_window_day_and_overnight);
    RUN_TEST(test_slice02_post_restore_daytime_and_nighttime_decisions);
    RUN_TEST(test_slice02_daily_schedule_transitions);
    RUN_TEST(test_slice03_ir_sequencer_lamp_white_3_steps);
    RUN_TEST(test_slice03_ir_sequencer_lamp_off_2_steps);
    RUN_TEST(test_slice03_ir_clone_3_slots);
    RUN_TEST(test_slice04_offline_queue_store_and_forward_blackout_flow);
    RUN_TEST(test_slice04_ring_buffer_overflow_overwrites_oldest);
    RUN_TEST(test_slice04_remote_commands_parser);
    RUN_TEST(test_slice05_feeder_pulse_duration_bounds);
    RUN_TEST(test_slice05_feeder_dispense_and_cooldown_and_battery_lock);
    RUN_TEST(test_slice06_feeder_remote_commands);
    RUN_TEST(test_slice07_battery_percentage_and_commands);
    return UNITY_END();
}
