# PLAN-001: Plan de Trabajo por Vertical Slices — `ControladorPeceraDistancia`

> **Autor:** Agente `planificador` (`docs(arquitectura): definir arquitectura dual nano-esp32, cola offline y plan de 4 vertical slices`)  
> **Insumo de Hardware:** [`docs/hardware/bom-mercadolibre.md`](../hardware/bom-mercadolibre.md)  
> **Arquitectura de Referencia:** [`docs/architecture/como-funciona.md`](../architecture/como-funciona.md)  
> **Destinatarios:** `gestor-issues` (publicación de issues), `implementador` (codificación slice por slice), `tester` (validación `native` + compilación) y `documentador`.

---

## Contrato Integral de Interfaz C++ (`include/controller.h`)

Para preservar compatibilidad regresiva con los tests iniciales y construir los 4 Vertical Slices de forma incremental sin usar memoria dinámica (`0 bytes` de `heap`), `include/controller.h` evolucionará hasta exponer el siguiente contrato completo dentro de `namespace AppLogic`:

```cpp
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace AppLogic {

// Constantes de tiempo y capacidad (aptas para 2KB SRAM de Arduino Nano)
static const uint32_t DEFAULT_MAINS_DEBOUNCE_MS      = 500;
static const uint32_t DEFAULT_LAMP_BOOT_SETTLE_MS    = 3000;
static const uint32_t DEFAULT_IR_INTER_CMD_DELAY_MS  = 250;
static const uint8_t  OFFLINE_QUEUE_CAPACITY         = 16;

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

// SLICE-05: Dosificador de Emergencia por Tolva Calibrada en Milisegundos (~3 pellets)
bool setFeederPulseDuration(SystemState& state, uint16_t pulseMs);
bool triggerEmergencyFeed(SystemState& state, uint16_t currentRtcMin, uint32_t currentMs, bool forceIgnoreCooldown);
bool stepFeeder(SystemState& state, uint16_t currentRtcMin, uint32_t currentMs);

// SLICE-07: Medición de Voltaje y Porcentaje de Batería 18650 (ADC)
uint8_t computeBatteryPercent(uint16_t batteryMv);
void updateBatteryReading(SystemState& state, uint16_t batteryMv);

} // namespace AppLogic
```

---

## SLICE-01: Detección No Bloqueante de Corte/Retorno de 220V y Conmutación a Batería

- **Funcionalidad End-to-End:**
  - Muestrear `PIN_MAINS_SENSE` (`D2` en Arduino Nano / `GPIO 34` en ESP32), tomado antes del diodo Schottky `D1 (1N5819)` del cargador USB de `220V`.
  - Aplicar antirrebote no bloqueante de `500 ms` (`DEFAULT_MAINS_DEBOUNCE_MS`):
    - Si cae a `LOW` durante `>= 500 ms`, transiciona a `MAINS_LOST`, activa `onBatteryBackup = true` y registra `EVT_POWER_LOST`.
    - Si vuelve a `HIGH` durante `>= 500 ms`, transiciona a `MAINS_RESTORING_WAIT`, activa `onBatteryBackup = false` y registra `EVT_POWER_RESTORED`.
  - Mantener el estado `MAINS_RESTORING_WAIT` durante `3000 ms` (`DEFAULT_LAMP_BOOT_SETTLE_MS`) para dar tiempo a que la lámpara LED de la pecera encienda su receptor IR interno tras el regreso de `220V`, y al cumplirse activar `postRestoreActionPending = true` pasando a `MAINS_OK`.
- **Pines y Módulos del BOM involucrados:**
  - Cargador USB `5V` (`220V`) + 2x Diodos Schottky `1N5819` + Batería de respaldo (`3x AA/AAA` o Celda Litio + `TP4056`) + Divisor `10kΩ/20kΩ` hacia `D2` (Nano) / `GPIO 34` (ESP32).
- **Contratos en `include/controller.h`:**
  - `void updateMainsPower(SystemState& state, bool mainsPinHigh, uint16_t currentRtcMin, uint32_t currentMs);`
- **Archivos a Modificar por el `implementador`:**
  1. `platformio.ini` (habilitar entornos `[env:nanoatmega328]`, `[env:esp32dev]` y `[env:native]`)
  2. `include/controller.h`
  3. `src/controller.cpp`
  4. `src/main.cpp`
- **Criterios de Verificación para el `tester`:**
  - `pio test -e native`: Verifica que un microcorte `< 500 ms` se ignora; una caída `>= 500 ms` pasa a `MAINS_LOST` (`onBatteryBackup == true`); el retorno `>= 500 ms` pasa a `MAINS_RESTORING_WAIT`; y recién tras `3000 ms` adicionales pasa a `MAINS_OK` con `postRestoreActionPending == true`.
  - `pio run -e nanoatmega328` y `pio run -e esp32dev`: Compilación limpia (`RAM < 50%` en Arduino Nano).
- **Conventional Commit Asignado:**
  - Implementación: `feat(energia): detectar corte y retorno de 220v con retardo de estabilizacion`
  - Documentación: `docs(energia): documentar circuito hibrido 1n5819 y deteccion de corte 220v`

---

## SLICE-02: Fotoperíodo Diario (`RTC DS3231`) y Decisión Automática Post-Corte

- **Funcionalidad End-to-End:**
  - Evaluar el minuto actual del día (`0..1439`, obtenido del `RTC DS3231`) frente a la ventana de iluminación configurada (por defecto `10:00` = `600` a `18:00` = `1080`, soportando también ventanas nocturnas que cruzan medianoche).
  - **Resolución automática del problema del modo multicolor post-corte:** Cuando `postRestoreActionPending == true` (acaba de volver la electricidad y ya pasaron los `3000 ms` de arranque de la lámpara):
    - Si la hora actual está **dentro** del fotoperíodo (ej. `14:30`), decide `targetLampMode = LAMP_WHITE` para sacar a la lámpara del modo multicolor y fijarla en luz blanca.
    - Si la hora actual está **fuera** del fotoperíodo (ej. `03:00 AM`), decide `targetLampMode = LAMP_OFF` para apagar la lámpara inmediatamente.
  - **Transiciones diarias:** En operación normal (`MAINS_OK`, `POLICY_AUTO_RTC`), al entrar en la ventana horaria solicita `LAMP_WHITE` (`EVT_SCHEDULE_ON`) y al salir solicita `LAMP_OFF` (`EVT_SCHEDULE_OFF`).
- **Pines y Módulos del BOM involucrados:**
  - Módulo `RTC DS3231` con pila `CR2032` en bus `I2C` (`A4/A5` en Arduino Nano, `GPIO 21/22` en ESP32).
- **Contratos en `include/controller.h`:**
  - `bool isMinuteInWindow(uint16_t currentMin, uint16_t startMin, uint16_t endMin);`
  - `void setPhotoperiodSchedule(SystemState& state, uint16_t startMin, uint16_t endMin);`
  - `void evaluatePhotoperiod(SystemState& state, uint16_t currentRtcMin);`
- **Archivos a Modificar por el `implementador`:**
  1. `include/controller.h`
  2. `src/controller.cpp`
  3. `src/main.cpp`
- **Criterios de Verificación para el `tester`:**
  - `pio test -e native`: Verifica retorno de corte diurno (`14:30` $\rightarrow$ `LAMP_WHITE`), retorno de corte nocturno (`03:00` $\rightarrow$ `LAMP_OFF`), cruce de hora de encendido (`09:59` $\rightarrow$ `10:00`) y cruce de hora de apagado (`17:59` $\rightarrow$ `18:00`).
  - `pio run -e nanoatmega328` y `pio run -e esp32dev`: Compilación exitosa dentro del presupuesto de `2 KB` de SRAM.
- **Conventional Commit Asignado:**
  - Implementación: `feat(fotoperiodo): evaluar ventana horaria rtc y decidir estado de luz post-corte`
  - Documentación: `docs(fotoperiodo): documentar configuracion de horario rtc y logica post-apagon`

---

## SLICE-03: Secuenciador Infrarrojo (`IR TX`) No Bloqueante y Clonador de Control (`IR RX`)

- **Funcionalidad End-to-End:**
  - **Secuenciador IR No Bloqueante (`stepIrSequencer`):**
    - Para aplicar **`LAMP_WHITE`** (corrigiendo el modo multicolor en que enciende la lámpara o encendiéndola desde apagado):
      1. Paso 1 (`t = 0 ms`): emite `IR_CMD_ON` (`irCodes.codeOn`).
      2. Paso 2 (`t = +250 ms`): emite `IR_CMD_WHITE` (`irCodes.codeWhite`).
      3. Paso 3 (`t = +500 ms`): re-emite `IR_CMD_WHITE` por seguridad, marca `appliedLampMode = LAMP_WHITE` y encola `EVT_LAMP_WHITE_APPLIED`.
    - Para aplicar **`LAMP_OFF`**:
      1. Paso 1 (`t = 0 ms`): emite `IR_CMD_OFF` (`irCodes.codeOff`).
      2. Paso 2 (`t = +250 ms`): re-emite `IR_CMD_OFF` por seguridad, marca `appliedLampMode = LAMP_OFF` y encola `EVT_LAMP_OFF_APPLIED`.
  - **Clonador de Control Remoto Original (`startIrClone` / `applyCapturedIrCode`):**
    - Permite capturar desde el receptor `KY-022` (`D4` en Nano / `GPIO 15` en ESP32) los 3 botones del control remoto original de la pecera (`ON`, `BLANCO`, `OFF`) y persistirlos en `EEPROM` (Nano) / `NVS` (ESP32).
- **Pines y Módulos del BOM involucrados:**
  - Emisor IR `KY-005` (`940nm`) + Transistor `2N2222` en `D3` (Nano) / `GPIO 4` (ESP32).
  - Receptor IR `KY-022` (`VS1838B 38kHz`) en `D4` (Nano) / `GPIO 15` (ESP32).
- **Contratos en `include/controller.h`:**
  - `void requestLampTransition(SystemState& state, LampTargetMode desiredMode, uint32_t currentMs);`
  - `IrCommandType stepIrSequencer(SystemState& state, uint32_t currentMs, uint32_t* outRawCode = nullptr);`
  - `void startIrClone(SystemState& state, IrCloneSlot slot);`
  - `bool applyCapturedIrCode(SystemState& state, uint32_t capturedCode);`
- **Archivos a Modificar por el `implementador`:**
  1. `include/controller.h`
  2. `src/controller.cpp`
  3. `src/main.cpp`
- **Criterios de Verificación para el `tester`:**
  - `pio test -e native`: Verifica que al pedir `LAMP_WHITE` se emiten exactamente `IR_CMD_ON` en `t=0`, `IR_CMD_WHITE` en `t=250ms` e `IR_CMD_WHITE` en `t=500ms` sin bloquear antes de cada plazo; verifica que al pedir `LAMP_OFF` se emite doble `IR_CMD_OFF` separado por `250ms`; y verifica el clonado de los 3 slots (`CLONE_WAIT_ON`, `CLONE_WAIT_WHITE`, `CLONE_WAIT_OFF`).
  - `pio run -e nanoatmega328` y `pio run -e esp32dev`: Compilación limpia.
- **Conventional Commit Asignado:**
  - Implementación: `feat(control-ir): implementar secuenciador ir no bloqueante y clonado de control`
  - Documentación: `docs(control-ir): documentar conexionado ky-005 ky-022 y clonado del control remoto`

---

## SLICE-04: Cola Circular Offline (*Store-and-Forward*), Captura de Cámara y Envío Diferido a Telegram

- **Funcionalidad End-to-End:**
  - **Cola Circular Offline Estática (`EventQueue` de `16` eventos en RAM fija, `< 110 bytes`):**
    - Cuando se corta la luz (`220V`), el router Wi-Fi de la casa se apaga y el celular Android (sin tarjeta SIM) no tiene internet.
    - Todos los eventos (`EVT_POWER_LOST`, `EVT_POWER_RESTORED`, `EVT_LAMP_WHITE_APPLIED`, `EVT_LAMP_OFF_APPLIED`, `EVT_SCHEDULE_ON`, `EVT_SCHEDULE_OFF`, `EVT_PHOTO_REQUESTED`) se almacenan en orden cronológico en `eventQueue` con su minuto exacto del `RTC DS3231` y el flag `localCameraTriggerPending = true` para que la cámara local capture la foto del momento.
    - Mientras `internetAvailable == false`, `dispatchNextQueuedEvent()` retiene todos los eventos sin perderlos.
    - Apenas vuelve la luz y el router doméstico recupera salida a internet (`internetAvailable == true`), `dispatchNextQueuedEvent()` extrae un evento por ciclo en orden FIFO y lo envía al Bot de Telegram junto con la foto de la pecera (permitiendo verificar a distancia el termómetro analógico, el chorro del filtro y que la luz haya quedado blanca o apagada).
  - **Comandos Remotos por `char[]` (`processRemoteCommand`):**
    - `/estado`: Devuelve estado de `220V`/batería, modo actual de la lámpara (`BLANCA`/`OFF`), horario RTC y cantidad de eventos en cola.
    - `/cola`: Muestra el resumen de eventos pendientes de envío.
    - `/foto`: Encola `EVT_PHOTO_REQUESTED` y activa `localCameraTriggerPending`.
    - `/luz_blanca` y `/luz_off`: Pasan a `POLICY_MANUAL_OVERRIDE` y disparan la secuencia IR correspondiente.
    - `/auto`: Restaura `POLICY_AUTO_RTC` y evalúa el fotoperíodo actual.
    - `/horario HH:MM HH:MM`: Actualiza `scheduleStartMin` y `scheduleEndMin`.
    - `/clonar_ir`: Inicia el asistente de clonado IR (`CLONE_WAIT_ON`).
- **Pines y Módulos del BOM involucrados:**
  - Vínculo `Serial` (`TX/RX`) / `Wi-Fi` hacia Celular Android sin SIM (Hotspot local + app de cámara) o `ESP32-CAM`.
- **Contratos en `include/controller.h`:**
  - `bool pushOfflineEvent(SystemState& state, EventType type, uint16_t timestampMin, LampTargetMode mode, bool requestPhoto);`
  - `bool popOfflineEvent(SystemState& state, OfflineEvent& outEvent);`
  - `bool dispatchNextQueuedEvent(SystemState& state, bool internetReady, OfflineEvent& outDispatched);`
  - `bool processRemoteCommand(SystemState& state, const char* cmd, uint16_t currentRtcMin, uint32_t currentMs, char* replyBuf, size_t replyBufSize);`
- **Archivos a Modificar por el `implementador`:**
  1. `include/controller.h`
  2. `src/controller.cpp`
  3. `src/main.cpp`
- **Criterios de Verificación para el `tester`:**
  - `pio test -e native`:
    - Simula un corte de luz completo a las `02:15` (`internetAvailable = false`), retorno de luz a las `03:40` con disparo automático de `LAMP_OFF` (`IR_CMD_OFF`), verifica que los 3 eventos (`EVT_POWER_LOST`, `EVT_POWER_RESTORED`, `EVT_LAMP_OFF_APPLIED`) permanecen encolados mientras no hay Wi-Fi doméstico, y comprueba que al pasar `internetAvailable = true` se despachan en orden FIFO exacto con sus timestamps `02:15` y `03:40`.
    - Verifica el parser de comandos `/estado`, `/foto`, `/luz_blanca`, `/luz_off`, `/auto` y `/horario 09:30 19:00`.
  - `pio run -e nanoatmega328` y `pio run -e esp32dev`: Compilación limpia con uso de SRAM `< 50%` en Arduino Nano (`< 1024 bytes`).
- **Conventional Commit Asignado:**
  - Implementación: `feat(telemetria-cola): encolar eventos sin conexion y despachar a telegram al reconectar`
  - Documentación: `docs(telemetria-cola): documentar cola offline store-and-forward y comandos de telegram`

---

## SLICE-05: Máquina de Estados No Bloqueante del Alimentador de Emergencia en `ms` (~3 Pellets)

- **Funcionalidad End-to-End:**
  - Controlar el motor/servomotor de la tolva de emergencia (`GPIO 1` en `ESP32-C3 SuperMini`, `GPIO 25` en `ESP32`, `D5` en `Arduino Nano`) mediante un pulso no bloqueante de duración calibrada `feederPulseMs` (por defecto `350 ms`, rango permitido `50..2000 ms` para liberar exactamente ~3 pellets de fondo).
  - **Protecciones críticas de seguridad:**
    1. Tope duro: `setFeederPulseDuration()` rechaza cualquier valor `< 50 ms` o `> 2000 ms` (`MAX_FEEDER_PULSE_MS`).
    2. Bloqueo en apagón: `triggerEmergencyFeed()` rechaza accionar el motor si `onBatteryBackup == true` (`MAINS_LOST`) para nunca descargar la celda `Sanyo UR18650ZT`.
    3. *Cooldown* anti-doble disparo: tras iniciar o completar una descarga, bloquea nuevos disparos durante `feederCooldownMs` (`60000 ms` por defecto) salvo que `forceIgnoreCooldown == true` (modo calibración en casa).
  - Al finalizar el pulso de `feederPulseMs` en `stepFeeder()`, apaga/cierra el motor (`feederOutputActive = false`), incrementa `feedDispensedCount++`, pasa a `FEEDER_COOLDOWN` y encola automáticamente `EVT_FEED_DISPENSED` con `photoRequested = true` para confirmar visualmente por cámara que cayeron los 3 pellets.
  - Además, pre-carga en `initState()` los códigos NEC universales del control remoto de 24 teclas (`ON = 0x00F7C03F`, `WHITE = 0x00F7E01F`, `OFF = 0x00F740BF`).
- **Archivos a Modificar por el `implementador`:**
  1. `include/controller.h`
  2. `src/controller.cpp`
  3. `src/main.cpp`
- **Criterios de Verificación para el `tester`:**
  - Tests unitarios nativos validando validación de rango `50..2000 ms`, pulso exacto de `350 ms`, bloqueo en batería (`onBatteryBackup == true`), bloqueo por *cooldown*, bypass con `forceIgnoreCooldown == true` y encolado de `EVT_FEED_DISPENSED` con `photoRequested == true`.
  - Compilación limpia (`0 warnings`, `RAM < 50%` en Arduino Nano).
- **Conventional Commit Asignado:**
  - Implementación: `feat(alimentador): implementar dosificador no bloqueante en ms con cooldown y seguridad (Closes #5)`
  - Documentación: `docs(alimentador): documentar conexionado de servo o motor dc y protecciones de tolva (Refs #5)`

---

## SLICE-06: Comandos Remotos `/alimentar`, `/alimentar_forzar` y `/calibrar_ms`

- **Funcionalidad End-to-End:**
  - Extender `processRemoteCommand()` en `src/controller.cpp` (con buffers `char[]` estáticos):
    - `/calibrar_ms <50..2000>`: ajusta `feederPulseMs` en vivo para calibrar la caída de los 3 pellets de fondo.
    - `/alimentar`: ejecuta `triggerEmergencyFeed(state, currentRtcMin, currentMs, false)` respetando el *cooldown* y el bloqueo en corte de luz.
    - `/alimentar_forzar`: ejecuta `triggerEmergencyFeed(state, currentRtcMin, currentMs, true)` ignorando el *cooldown* para pruebas de calibración con plato en casa.
    - Actualizar `/estado` para reportar `FEED_MS=<ms>` y `DOSIS=<N>`.
- **Archivos a Modificar por el `implementador`:**
  1. `include/controller.h`
  2. `src/controller.cpp`
  3. `src/main.cpp`
- **Criterios de Verificación para el `tester`:**
  - Tests unitarios nativos verificando `/calibrar_ms 420`, rechazo de `/calibrar_ms 5000`, disparo con `/alimentar`, bloqueo de segundo `/alimentar` inmediato por *cooldown*, éxito con `/alimentar_forzar` y presencia de `DOSIS=` en `/estado`.
- **Conventional Commit Asignado:**
  - Implementación: `feat(alimentador-cmd): agregar comandos /alimentar y /calibrar_ms con telemetria (Closes #6)`
  - Documentación: `docs(alimentador-cmd): documentar calibracion de 3 pellets y comandos de alimentacion (Refs #6)`

---

## SLICE-07: Medición de Voltaje y Porcentaje de Batería `UR18650ZT` (`ADC` + `/bateria`)

- **Funcionalidad End-to-End:**
  - Leer el voltaje real de la celda `Sanyo UR18650ZT` (`3.00V` = `0%` a `4.20V` = `100%`) mediante un divisor resistivo de **2 resistencias iguales** (ej. `2x 22kΩ` o `2x 4.7kΩ`) desde **`OUT+` del `TP4056`** hacia **`GPIO 0` (`ADC1`)** en `ESP32-C3 SuperMini` (`GPIO 35` en `ESP32`, `A0` en `Arduino Nano`).
  - **Lógica Pura (`include/controller.h` + `src/controller.cpp`):**
    - Constantes: `BATTERY_EMPTY_MV = 3000`, `BATTERY_FULL_MV = 4200`.
    - Campos en `SystemState`: `uint16_t batteryMv; uint8_t batteryPercent;`.
    - `uint8_t computeBatteryPercent(uint16_t batteryMv)`: convierte milivoltios a `0..100%` acotado (`<= 3000 mV -> 0%`, `>= 4200 mV -> 100%`).
    - `void updateBatteryReading(SystemState& state, uint16_t batteryMv)`: actualiza `state.batteryMv` y `state.batteryPercent`.
    - Comandos remotos: nuevo comando **`/bateria`** (devuelve `BAT=X.XXV (YY%)`), suma `BAT=YY%` a **`/estado`** y agrega `/bateria` a **`/help`**.
  - **Capa HAL (`src/main.cpp`):** Muestreo no bloqueante cada `1000 ms` por `analogReadMilliVolts(PIN_BAT_ADC) * 2` (en ESP32) o `(analogRead(A0) * 5000UL * 2UL) / 1023UL` (en Nano).
- **Archivos a Modificar por el `implementador`:**
  1. `include/controller.h`
  2. `src/controller.cpp`
  3. `src/main.cpp`
- **Criterios de Verificación para el `tester`:**
  - Tests unitarios nativos validando `computeBatteryPercent` (`2800mV -> 0%`, `3000mV -> 0%`, `3600mV -> 50%`, `4200mV -> 100%`, `4300mV -> 100%`), `updateBatteryReading`, `/bateria` y presencia de `BAT=` en `/estado`.
  - Compilación limpia (`0 warnings`, `RAM < 65%` en Arduino Nano).
- **Conventional Commit Asignado:**
  - Implementación: `feat(bateria): medir voltaje de celda 18650 y reportar porcentaje en /estado y /bateria (Closes #7)`
  - Documentación: `docs(bateria): documentar divisor adc en gpio 0 y comando /bateria (Refs #7)`

---

## SLICE-08: Refactor de Mensajería Telegram con Emojis, Formato Limpio y Cobertura Total de Comandos

- **Funcionalidad End-to-End:**
  - **Refactor de respuestas y alertas (`src/controller.cpp` + `src/main.cpp`):**
    - Reemplazar textos técnicos crudos (`Evento=3 Luz=2 Foto=1`, `220V:OK LUZ:OFF COLA:0`) por tarjetas limpias multilínea con emojis (`⚡`, `🔋`, `💡`, `🐟`, `📸`, `⏰`, `📶`, `🌡️`, `🟢`, `🔴`, `🟡`), fáciles de leer en pantalla de celular sin tablas rotas.
    - Garantizar que **todo subsistema del controlador** tenga su comando dedicado y respuesta clara:
      - Energía y batería: `/estado`, `/bateria`, `/cola`
      - Iluminación y fotoperíodo: `/luz_on`, `/luz_blanca`, `/luz_off`, `/auto`, `/horario`, `/clonar_ir`
      - Alimentador de emergencia: `/alimentar`, `/alimentar_forzar`, `/calibrar_ms`
      - Cámara y monitoreo: `/foto`, `/foto_flash`, `/live`, `/cam_ip`
      - Ayuda y referencia: `/help` (`/ayuda`), `/eventos`
    - Mantener compatibilidad regresiva con los 17 tests unitarios existentes (`test/test_native/test_main.cpp`) y agregar tests de verificación de formato legible.
    - Optimizar buffers `char[]` estáticos y `snprintf` en `src/main.cpp` para mantener bajo consumo de RAM (`< 15%` en ESP32-C3 / `< 65%` en Arduino Nano).
- **Archivos a Modificar por el `implementador`:**
  1. `src/controller.cpp`
  2. `src/main.cpp`
- **Criterios de Verificación para el `tester`:**
  - `17+` tests unitarios nativos en `PASS` (`0 Failures`, `0 warnings`).
  - Compilación limpia para `ESP32-C3` y `Arduino Nano`.
- **Conventional Commit Asignado:**
  - Implementación: `refactor(mensajes): mejorar formato de alertas y respuestas telegram con emojis y cobertura total (Closes #8)`
  - Documentación: `docs(mensajes): actualizar guia de comandos telegram y menu botfather (Refs #8)`
