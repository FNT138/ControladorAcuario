# Manual de Uso y Funcionalidades — Controlador de Pecera a Distancia

> **Mantenido exclusivamente por:** Agente `documentador`

---

## 1. Funcionalidades Actuales

### Base
- **Control por Umbral No Bloqueante (`evaluateThreshold`):** Muestrea el sensor analógico usando `millis()` y activa la salida digital si la lectura iguala o supera el umbral configurado.

### SLICE-01: Detección No Bloqueante de Corte/Retorno 220V y Respaldo a Batería (Issue #1)
Implementado en [`include/controller.h`](../../include/controller.h) mediante `AppLogic::updateMainsPower()` y la máquina de estados `AppLogic::MainsStatus`:
- **Antirrebote de corte/retorno (`DEFAULT_MAINS_DEBOUNCE_MS = 500 ms`):**
  - Ignora microcortes de tensión menores a `500 ms`.
  - Si el pin `MAINS_SENSE` permanece en `LOW` durante `>= 500 ms`, pasa de `MAINS_OK` $\rightarrow$ `MAINS_DEBOUNCE_LOST` $\rightarrow$ `MAINS_LOST` y activa `onBatteryBackup = true`.
  - Cuando vuelve la energía `220V` (`HIGH`) durante `>= 500 ms`, pasa por `MAINS_DEBOUNCE_RESTORED` $\rightarrow$ `MAINS_RESTORING_WAIT` y desactiva `onBatteryBackup = false`.
- **Estabilización de lámpara post-corte (`DEFAULT_LAMP_BOOT_SETTLE_MS = 3000 ms`):**
  - Al retornar los `220V`, mantiene el estado `MAINS_RESTORING_WAIT` durante `3000 ms` sin bloquear el `loop()`, dando tiempo a que el receptor IR interno de la lámpara LED de la pecera inicialice.
  - Cumplidos los `3000 ms`, pasa a `MAINS_OK` y activa `postRestoreActionPending = true` para que los módulos de control horario/IR corrijan el estado de la lámpara.

### SLICE-02: Fotoperíodo Diario (`RTC DS3231`) y Decisión Automática Post-Corte (Issue #2)
Implementado en [`include/controller.h`](../../include/controller.h) mediante `isMinuteInWindow()`, `setPhotoperiodSchedule()` y `evaluatePhotoperiod()`:
- **Ventana horaria configurable (`0..1439` min desde medianoche):**
  - Horario por defecto: **`10:00` (`600`) a `18:00` (`1080`)**.
  - Soporta tanto ventanas **diurnas** (`startMin < endMin`, ej. `10:00–18:00`) como ventanas **nocturnas** que cruzan la medianoche (`startMin > endMin`, ej. `22:00–06:00`).
- **Resolución automática post-corte (`postRestoreActionPending == true`):**
  - Cuando vuelve la electricidad (`220V`) y finaliza el retardo de `3000 ms`, consulta la hora actual del `RTC DS3231` sin depender de Wi-Fi:
    - **Retorno diurno (dentro del fotoperíodo, ej. `14:30`):** decide `targetLampMode = LAMP_WHITE` para sacar la lámpara del modo multicolor automático tras el corte y fijarla en luz blanca.
    - **Retorno nocturno (fuera del fotoperíodo, ej. `03:00`):** decide `targetLampMode = LAMP_OFF` para apagar la lámpara inmediatamente.
- **Transiciones horarias diarias (`MAINS_OK`, `POLICY_AUTO_RTC`):**
  - Al cruzar la hora de inicio (ej. `09:59` $\rightarrow$ `10:00`) solicita `LAMP_WHITE`; al cruzar la hora de fin (ej. `17:59` $\rightarrow$ `18:00`) solicita `LAMP_OFF`.

---

## 2. Tabla de Pines por Placa (`SLICE-01` + `SLICE-02`)

| Señal / Función | ESP32-C3 SuperMini | ESP32 Clásico (NodeMCU-32S / DevKit) | Arduino Nano V3 | Notas Eléctricas |
| :--- | :---: | :---: | :---: | :--- |
| **`PIN_MAINS_SENSE`** (Detección 220V) | **`GPIO 3`** | **`GPIO 34`** *(solo entrada)* | **`D2`** *(INT0)* | Conectado al punto medio del divisor `10kΩ / 20kΩ` (`HIGH` ~`3.3V` = 220V OK, `LOW` `0V` = Corte) |
| **`RTC DS3231` — `SDA`** (Bus I2C `0x68`) | **`GPIO 6`** | **`GPIO 21`** | **`A4`** | Línea de datos I2C hacia el módulo `RTC DS3231` (`ZS-042` con pila `CR2032`) |
| **`RTC DS3231` — `SCL`** (Bus I2C `0x68`) | **`GPIO 7`** | **`GPIO 22`** | **`A5`** | Línea de reloj I2C hacia el módulo `RTC DS3231` |
| **Alimentación `RTC DS3231` (`VCC`)** | `3.3V` | `3.3V` | `5V` | Alimentación lógica compatible con los niveles I2C de cada placa |
| **Alimentación principal `5V`** | Pin `5V` | Pin `5V` / `VIN` | Pin `5V` | Recibe `~4.7V–4.8V` desde los cátodos unidos de `D1` (USB 220V) y `D2` (Respaldo batería) |
| **Masa común (`GND`)** | `GND` | `GND` | `GND` | Compartida entre cargador 5V, `TP4056`, `MT3608`, divisor resistivo, `RTC DS3231` y placa |

---

## 3. Conexionado del Respaldo Híbrido (UR18650ZT + TP4056 + MT3608 + Diodos 1N5819/1N5822)

### A. Prueba Previa de Celdas `UR18650ZT`
1. Medir tensión en vacío con multímetro:
   - **$< 2.0\text{V}$:** Descartar (descarga profunda). Como alternativa sin litio, usar portapilas **`3x AA/AAA` (`4.5V`)** conectado directo al ánodo de `D2`.
   - **$2.5\text{V} - 4.2\text{V}$:** Recuperable. Cargar en el `TP4056` hasta que el LED indique carga completa (`4.20V`, sin calentarse) y verificar tras **24 horas** en reposo que conserve **$\ge 4.05\text{V}$**.

### B. Conexión Paso a Paso de la Etapa de Energía
1. **Cargador USB `5V` (220V) $\rightarrow$ Diodo `D1` y `TP4056`:**
   - Conectar `+5V` del cargador al **ánodo de `D1`** (`1N5819` para Nano, o `1N5822` de `3A` para ESP32). El **cátodo de `D1`** va al bus `5V` del microcontrolador.
   - En paralelo (tomado **antes** de `D1`), conectar `+5V` al terminal **`IN+`** del módulo `TP4056` (versión de **6 terminales** con chip de protección `DW01A`) y `GND` a **`IN-`**.
2. **Celda `UR18650ZT` $\rightarrow$ `TP4056` (`B+` / `B-`):**
   - Conectar el polo positivo de la celda (o celdas en paralelo) a **`B+`** y el negativo a **`B-`** del `TP4056`, respetando estrictamente la polaridad.
3. **Elevador `MT3608` $\rightarrow$ `TP4056` (`OUT+` / `OUT-`) y Diodo `D2`:**
   - Conectar **`OUT+`** del `TP4056` al terminal **`IN+`** del step-up `MT3608`, y **`OUT-`** del `TP4056` a **`IN-`** del `MT3608`.
   - **Calibración previa:** Antes de conectar el microcontrolador, ajustar el potenciómetro del `MT3608` midiendo con multímetro hasta fijar **`4.8V`** estables en su salida (`VOUT+`/`VOUT-`).
   - Conectar `VOUT+` del `MT3608` al **ánodo de `D2`** (`1N5819` o `1N5822` para ESP32). Unir el **cátodo de `D2`** con el **cátodo de `D1`** en el pin `5V` de la placa.
   - *(Recomendado en ESP32)*: Colocar un capacitor electrolítico de **`470µF`** entre `5V` y `GND` del microcontrolador para absorber picos de transmisión Wi-Fi.
4. **Detector de Corte 220V (Divisor Resistivo):**
   - Tomar el nodo de `+5V` **antes del ánodo de `D1`** (así `D1` impide que la batería retroalimente el sensor durante un corte).
   - Conectar **`R1 = 10kΩ`** entre ese nodo `+5V` y `PIN_MAINS_SENSE` (`GPIO 3` / `GPIO 34` / `D2`).
   - Conectar **`R2 = 20kΩ`** entre `PIN_MAINS_SENSE` y `GND`. Esto reduce los `5.0V` a **`~3.33V`** seguros para las entradas de `3.3V` del ESP32 y suficientes como `HIGH` lógico en Arduino Nano.

```text
[Cargador USB 220V (5V)] ──┬──► [Ánodo D1 (1N5819/1N5822)] ──► [Cátodo D1] ──┬──► Pin 5V Placa (+ Cap 470µF en ESP32)
                           │                                                 │
                           ├──► [R1 10kΩ] ──┬──► PIN_MAINS_SENSE             │
                           │                └──► [R2 20kΩ] ──► GND           │
                           │                                                 │
                           └──► [IN+ TP4056] (6 pines)                       │
                                 │        │                                  │
                     [Celda UR18650ZT]  [OUT+/OUT- TP4056]                   │
                      (en B+ / B-)        │                                  │
                                     [MT3608 (4.8V)] ──► [Ánodo D2] ─────────┘
```

---

## 4. Puesta en Marcha y Verificación
1. Verificar con multímetro que sin cargador USB (`220V` desconectado) el pin `MAINS_SENSE` marque `0V` y la placa siga encendida desde la batería (`~4.3V–4.5V` tras `D2`).
2. Abrir el monitor serie a **`115200 baudios`** para verificar el arranque del controlador y las transiciones de estado ante desconexión/reconexión del cargador USB.

