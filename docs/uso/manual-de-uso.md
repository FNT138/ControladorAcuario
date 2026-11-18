# Manual de Uso y Funcionalidades

> **Mantenido exclusivamente por:** Agente `documentador`

## 1. Funcionalidades Actuales
- **Control por Umbral No Bloqueante:** Muestrea el sensor analógico cada `100 ms` usando `millis()` y activa la salida digital si la lectura iguala o supera `512`.

## 2. Conexiones y Puesta en Marcha
1. Conectar la salida del sensor analógico al pin `A0` (y `VCC`/`GND`).
2. Conectar el actuador o relé al pin digital `D13`.
3. Abrir el monitor serie a `115200 baudios` para verificar el mensaje `[BOOT] Sistema Arduino Inicializado`.
