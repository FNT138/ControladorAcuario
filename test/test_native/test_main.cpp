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

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_clean);
    RUN_TEST(test_threshold_activation_after_interval);
    RUN_TEST(test_slice01_ignore_glitch_under_500ms);
    RUN_TEST(test_slice01_detect_power_loss_and_restore_after_3000ms);
    RUN_TEST(test_slice02_photoperiod_window_day_and_overnight);
    RUN_TEST(test_slice02_post_restore_daytime_and_nighttime_decisions);
    RUN_TEST(test_slice02_daily_schedule_transitions);
    return UNITY_END();
}
