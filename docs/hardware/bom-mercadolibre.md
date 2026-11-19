# Bill of Materials (BOM) Definitivo — `ControladorPeceraDistancia` (Mercado Libre Argentina)

> **Mantenido por:** Agente `hardware` (`docs(hardware): consolidar bom minimalista con esp32-c3 supermini y componentes reciclados`)  
> **Criterio:** Máximo ahorro reutilizando las partes que ya tenés en casa (`Sanyo UR18650ZT`, LED IR de control de 24 teclas, transistor `8050SS`, diodos `1N4007`, resistencias `22kΩ`/`4.5kΩ`, servomotor/motor DC y celular Android sin SIM).

---

## 1. Lista de Compras Cerrada en Mercado Libre Argentina (Solo 5 Ítems)

| # | Cant. | Componente a Comprar | Término de Búsqueda en MeLi AR | Precio Est. ($ ARS) | Función en el Proyecto |
| :-: | :---: | :--- | :--- | :---: | :--- |
| **1** | **1x** | **`ESP32-C3 SuperMini` (16 pines, USB-C)** | `esp32 c3 supermini wifi bluetooth` | `~$7.500` | Microcontrolador Wi-Fi con **RTC interno** (sincronizado por NTP) + 11 GPIOs (reutilizable luego en `AquaNode`) |
| **2** | **1x** | **Módulo Cargador `TP4056` con Protección (`DW01-A`)** | `modulo cargador bateria litio tp4056 con proteccion` | `~$3.200` | Carga tu celda `UR18650ZT` desde `5V` USB y la protege con PCM integrado (`B+/B-` separados de `OUT+/OUT-`) |
| **3** | **1x** | **Módulo Elevador Step-Up `MT3608` (`2A`)** | `modulo step up mt3608` | `~$2.300` | Eleva los `3.7V` de `OUT+` del `TP4056` a **`4.7V`** estables antes del diodo `D2` |
| **4** | **1x** | **Zócalo / Portapilas `1x 18650` con cables** | `portapilas 18650 1 slot con cables` | `~$1.500` | Aloja 1 celda `Sanyo UR18650ZT` conectada a `B+` y `B-` del `TP4056` |
| **5** | **1x** | **Pack Cables Dupont** *(+ Protoboard si no tenés)* | `cables dupont macho hembra arduino` | `~$3.500` | Interconexión en protoboard dentro de caja plástica estanca |
| — | — | **TOTAL A COMPRAR** | *(Agrupando en carrito Full)* | **`~$18.000 ARS`** | **Ahorro de >$25.000 ARS** gracias a los componentes reciclados en casa |

---

## 2. Inventario de Componentes Ya Disponibles en Casa (`$0 ARS`)

| Cant. | Componente Disponible en Casa | Dónde se Conecta | Rol en el Circuito |
| :---: | :--- | :--- | :--- |
| **1x** | **Celda `Sanyo UR18650ZT` (`> 3.0V`, reciclada de laptop)** | `B+` y `B-` del módulo `TP4056` | Brinda **~35 a 45 horas** de autonomía en apagón sin que el `ESP32-C3` pierda la hora de su RTC interno *(quedan 2 celdas de repuesto)* |
| **1x** | **LED Emisor Infrarrojo `940nm`** *(desoldado de control viejo de 24 teclas)* | Colector del transistor `8050SS` y `+5V` vía resistencia `47Ω..330Ω` | Dispara los códigos universales NEC del control de 24 teclas (`ON = 0xF7C03F`, `W = 0xF7E01F`, `OFF = 0xF740BF`) |
| **1x** | **Transistor NPN `8050SS` (`S8050`, `1.5A`)** *(+ `BC546` / `C945` de repuesto)* | `E` $\rightarrow$ `GND`, `B` $\rightarrow$ `R 4.5kΩ` a `GPIO 4`, `C` $\rightarrow$ Cátodo (`-`) del LED IR | Amplifica la señal de `3.3V` del `ESP32-C3` para darle máximo alcance al LED Infrarrojo *(y un 2.º `8050SS` si se usa motor DC en la tolva)* |
| **2x** | **Diodos `1N4007` (`1A`)** | **`D1`**: salida `5.0V` USB (`-> ~4.3V`). **`D2`**: salida `4.7V` del `MT3608` (`-> ~4.0V`) | Conmutación automática en `0 ms` al pin `5V` del `ESP32-C3` (`0 mA` de gasto de batería mientras hay `220V`) |
| **2x** | **Resistencias de `22kΩ`** | En serie entre `5.0V` USB (antes de `D1`) y `GND`; punto medio a **`GPIO 3`** | Divisor de tensión a la mitad (`5.0V -> 2.5V`) para detectar corte y retorno de `220V` sin exceder los `3.3V` del `ESP32-C3` |
| **2x** | **Resistencias de `4.5kΩ`** | En paralelo (`2.25kΩ`) o simple (`4.5kΩ`) entre **`GPIO 4`** y la Base del `8050SS` | Limita corriente de base del transistor `8050SS` |
| **1x** | **Resistencia de `47Ω a 330Ω`** *(medida entre "otras" o del control viejo / `LM393`)* | Entre `+5V` y el Ánodo (`+`, chapita interna chica) del LED Infrarrojo | Limita la corriente pico de los pulsos de `38 kHz` del LED IR |
| **1x** | **Servomotor de 3 cables (`SG90`/`MG995`)** *(o Motor DC de 2 cables)* | Señal a **`GPIO 1`** (`ESP32-C3`) / **`D5`** (`Nano`); `+5V` tomado **antes de `D1`** | **Alimentador de emergencia:** abre la tolva durante `feederPulseMs` milisegundos calibrados para liberar **~3 pellets de fondo** |
| **1x** | **Celular Android sin SIM + Cargador `220V`** | Apuntando cámara a la pecera (Wi-Fi casa / Hotspot local en apagón) | Monitoreo visual de termómetro, calentador, filtro y confirmación por foto tras `/alimentar` |

---

## 3. Esquema Eléctrico Final con Tus Componentes (`1N4007` + `2x 22kΩ` + `8050SS` + `UR18650ZT`)

```text
[Cargador USB 220V (5.0V)] ───┬───► [Ánodo D1 (1N4007)] ──► (~4.3V con 220V) ──┬──► Pin 5V (ESP32-C3 SuperMini)
                              │                                                │
                              ├───► [+5V Motor Alimentador (No gasta batería)] │
                              │                                                │
                              ├───► [R1 22kΩ] ──┬──► GPIO 3 (PIN_MAINS_SENSE)  │
                              │                 └──► [R2 22kΩ] ──► GND         │
                              │                                                │
                              └───► [IN+ TP4056]                               │
                                     ├─ B+/B- ──► [Celda Sanyo UR18650ZT]      │
                                     └─ OUT+ ───► [VIN+ MT3608 (reg. a 4.7V)]  │
                                                   └─ Vout+ ──► [D2 (1N4007)] ─┘ (~4.0V en corte)
```
