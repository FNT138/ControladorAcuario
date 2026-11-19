#include <unity.h>
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
    return UNITY_END();
}
