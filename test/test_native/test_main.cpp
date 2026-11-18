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

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_clean);
    RUN_TEST(test_threshold_activation_after_interval);
    return UNITY_END();
}
