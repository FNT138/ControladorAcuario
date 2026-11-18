#pragma once
#include <stdint.h>
#include <stdbool.h>

namespace AppLogic {

struct SystemState {
    uint16_t sensorRawValue;
    bool actuatorActive;
    uint32_t lastUpdateMs;
};

// Inicializa el estado del controlador
void initState(SystemState& state);

// Evalúa la lógica de control sin bloquear (desacoplada del hardware para testeo nativo)
bool evaluateThreshold(SystemState& state, uint16_t sensorReading, uint16_t threshold, uint32_t currentMs, uint32_t intervalMs);

} // namespace AppLogic
