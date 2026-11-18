#include "controller.h"

namespace AppLogic {

void initState(SystemState& state) {
    state.sensorRawValue = 0;
    state.actuatorActive = false;
    state.lastUpdateMs = 0;
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

} // namespace AppLogic
