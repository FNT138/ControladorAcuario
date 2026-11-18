# Bill of Materials (BOM) — Controlador de Pecera a Distancia (Mercado Libre Argentina)

> **Generado por:** Agente `hardware` (`docs(hardware): actualizar bom meLi ar, comparativa y respaldo sin ups`)  
> **Criterio de Selección:** Componentes 100% genéricos, económicos y de entrega inmediata (Full/Flex) en Mercado Libre Argentina, priorizando reutilizar los **2 Arduino Nano**, las **celdas de litio guardadas** y el **Celular Android sin SIM** que ya tenés en casa.

---

## 1. Comparativa de las 3 Propuestas de Arquitectura de Hardware

| Criterio | Propuesta A: **Tu `Arduino Nano` + `Celular Android`** | Propuesta B: **Tu `Arduino Nano` + `ESP32-CAM`** | Propuesta C: **`ESP32 NodeMCU` + `Celular Android`** |
| :--- | :--- | :--- | :--- |
| **Microcontrolador(es)** | **1x Arduino Nano V3** *(Ya lo tenés: $0 ARS)* | **1x Arduino Nano V3** *(Ya lo tenés)* + **1x `ESP32-CAM con base MB`** | **1x `ESP32 NodeMCU-32S` (30 pines)** |
| **Cámara apuntando a pecera** | **Celular Android sin SIM** (foco y resolución superior para ver el termómetro, calentador y filtro) | **Cámara `OV2640` 2MP integrada** en la `ESP32-CAM` (+ slot `MicroSD` a bordo) | **Celular Android sin SIM** corriendo *IP Webcam* (`http://<ip>:8080/shot.jpg`) |
| **Vínculo entre Placa y Cámara** | Bluetooth **`HC-05`** (permite dejar el celular con su cargador original 220V siempre enchufado) o cable **USB OTG** | `UART Serial` (`TX/RX` con divisor `5V -> 3.3V`) entre tu Nano y la `ESP32-CAM` | **Wi-Fi**: usa el router de la casa, y si se corta la luz pasa al **Hotspot Wi-Fi local (sin internet)** del celular |
| **Comportamiento en Corte de Luz (Sin Wi-Fi doméstico)** | El **Nano** sigue activo con batería (`AA/AAA` o `Litio`), corrige/apaga el LED IR al volver 220V y encola eventos con hora del `RTC`. El celular guarda fotos y al volver el Wi-Fi manda todo a Telegram | El **Nano** sigue activo con batería, corrige el LED IR y guarda la cola de eventos. La `ESP32-CAM` guarda fotos en `MicroSD` y al volver el Wi-Fi doméstico vacía la cola e imágenes a Telegram | El **ESP32** sigue activo con batería, se conecta al Hotspot local del celular para pedirle la captura, encola eventos en Flash (`LittleFS`) y al volver el Wi-Fi doméstico envía todo a Telegram |
| **Advertencia de Hardware** | Si usás cable OTG común, el celular no carga batería al mismo tiempo; con módulo **`HC-05`** el celular queda siempre conectado a su cargador original | Comprar sí o sí la **`ESP32-CAM` con base `ESP32-CAM-MB` (USB CH340)** porque la placa suelta no trae puerto USB | Requiere divisor resistivo `5V -> 3.3V` en el sensor de 220V y diodo Schottky `1N5822 (3A)` o `1N5819` con capacitor `470µF` para los picos Wi-Fi |

---

## 2. Lista de Materiales (BOM) — Módulos Comunes y Opcionales en Mercado Libre AR

### A. Módulos Base (Obligatorios para cualquiera de las 3 propuestas)

| Componente / Rol | Módulo Genérico Recomendado | Término Exacto de Búsqueda en MeLi AR | Interfaz / Pines (`Nano` / `ESP32`) | Tensión / Consumo | Notas Técnicas |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Controlador Tiempo Real** | **Arduino Nano V3 (ATmega328P CH340)** *(Ya tenés 2 en casa)* | *(Disponible en casa — $0 ARS)* | GPIO `5V` / `UART` / `I2C` | `5V` (`~20 mA`) | Ideal para excitar el LED IR a `5V` y gastar muy poca batería en cortes |
| **Reloj Tiempo Real + EEPROM** | Módulo **`RTC DS3231` (`ZS-042`)** con pila `CR2032` | `modulo rtc ds3231 con pila arduino` | I2C (`0x68`): Nano `A4(SDA)/A5(SCL)` \| ESP32 `21/22` | `3.3V` o `5V` (`< 2 mA`) | Además del reloj exacto sin Wi-Fi, trae chip EEPROM **`AT24C32` (`4 KB`)** en la misma placa |
| **Emisor Infrarrojo (TX)** | Módulo Emisor IR **`KY-005`** (`LED 940nm`) + Transistor **`2N2222`** (o `BC547`) | `modulo emisor infrarrojo ky-005 arduino` | Nano: `D3` (Timer PWM) \| ESP32: `GPIO 4` | `5V` (pico `~100 mA` pulsado) | El transistor `2N2222` (con `R 1kΩ` en base y `R 100Ω` en colector) triplica el alcance hacia la pecera |
| **Receptor Infrarrojo (RX)** | Módulo Receptor IR **`KY-022`** (`VS1838B` `38 kHz`) | `modulo receptor infrarrojo ky-022 vs1838b` | Nano: `D4` \| ESP32: `GPIO 15` | `3.3V - 5V` (`< 2 mA`) | Se usa para clonar una sola vez los botones `ON`, `OFF` y `BLANCO` del control original |
| **Respaldo Híbrido (Litio)** | Módulo Cargador **`TP4056` con Protección (`6 pines`)** + Tu celda de litio guardada | `modulo cargador bateria litio tp4056 con proteccion` | Entrada `5V USB` / Salida `OUT+` a diodo `D2` | `4.2V` carga / corte `2.5V` | **Importante:** Pedir el de **6 terminales** (`IN+/-`, `B+/-`, `OUT+/-` con chip `DW01A`) |
| **Respaldo Híbrido (Pilas)** | **Portapilas `3x AA` o `3x AAA`** (`4.5V`) + **2x Diodos Schottky `1N5819`** (o `1N5822`) | `portapilas 3 aa con cables` / `diodo schottky 1n5819` | Conmutación automática al pin `5V` | `4.5V` (`0 mA` mientras hay 220V) | Si tus celdas de litio guardadas no retienen carga, conectás el portapilas `3x AA` directo al diodo `D2` |
| **Detector Corte 220V** | Divisor resistivo (**`R1 = 10kΩ`** + **`R2 = 20kΩ`**) tomado antes del diodo `D1` del USB | `pack resistencias 1/4w 10k` | Nano: `D2 (INT0)` \| ESP32: `GPIO 34` | `HIGH` (`3.3V`) = 220V OK / `LOW` (`0V`) = Corte | **$0 ARS extra:** aprovecha los `5V` del mismo cargador USB conectado a `220V` |

### B. Módulos de Conectividad Según la Propuesta Elegida (Elegir solo 1)

| Propuesta | Módulo a Comprar en MeLi AR | Término Exacto de Búsqueda en MeLi AR | Función |
| :--- | :--- | :--- | :--- |
| **Propuesta A** *(Nano + Celular)* | Módulo Bluetooth **`HC-05`** (o **`ESP8266 ESP-01S`** con adaptador) | `modulo bluetooth hc-05 arduino` | Comunica tu `Arduino Nano` sin cables con el celular Android (dejando al celular enchufado a su cargador original) |
| **Propuesta B** *(Nano + ESP32-CAM)* | **`ESP32-CAM` + Base `ESP32-CAM-MB` (con USB)** + Tarjeta `MicroSD 8GB/16GB` | `esp32 cam con base programadora mb ov2640` | Toma fotos de la pecera, las guarda en `MicroSD` si se corta el Wi-Fi y envía la cola del Nano a Telegram al volver internet |
| **Propuesta C** *(ESP32 + Celular)* | **`ESP32 NodeMCU-32S` (30 pines)** | `esp32 nodemcu 30 pines wifi bluetooth` | Reemplaza al Nano o trabaja solo; pide fotos por Wi-Fi al celular (`IP Webcam`), usa Hotspot local en corte y envía a Telegram al reconectar |

---

## 3. Esquema Eléctrico del Respaldo Híbrido Sin UPS y Detector de 220V

```text
[Cargador USB 220V (5.0V)] ───┬───► [Anodo D1 (1N5819)] ──► [Catodo D1] ──┬──► Pin 5V (Arduino Nano / ESP32)
                              │                                           │
                              ├───► [R1 10kΩ] ──┬──► Pin MAINS_SENSE      │
                              │                 └──► [R2 20kΩ] ──► GND    │
                              │                                           │
                              └───► [IN+ TP4056 (Opción Litio)]           │
                                     │       │                            │
                         [Tu Celda Litio]   [OUT+ TP4056]                 │
                                 o               │                        │
                         [Portapilas 3x AA] ─────┴──► [Anodo D2 (1N5819)] ┘
```

### Guía Rápida para Probar las Celdas de Litio que Tenés Guardadas en Casa
1. **Medición en vacío con multímetro:**
   - Si la celda mide **$< 2.0\text{V}$**: sufrió descarga profunda química; **no la uses** (pasá directo al portapilas de **`3x AA` alcalinas** en `D2`).
   - Si mide **entre $2.5\text{V}$ y $4.2\text{V}$**: está recuperable.
2. **Prueba de carga y retención con el módulo `TP4056`:**
   - Conectala a `B+` y `B-` del `TP4056`. Durante la carga no debe entibiarse ni calentarse.
   - Cuando el LED del `TP4056` pase a **azul/verde fijo (`4.20V`)**, desconectala y dejala reposar **24 horas**.
   - Si a las 24 hs mide **$\ge 4.05\text{V}$**, la celda está en excelente estado para respaldar el controlador durante horas de corte de luz.
