# Controlador de Pecera a Distancia (`ControladorPeceraDistancia`)

Controlador no bloqueante multi-placa (**Arduino Nano V3**, **ESP32 Clásico** y **ESP32-C3 SuperMini**) para automatización de iluminación LED IR de pecera con respaldo de energía ante cortes de `220V`, reloj `RTC DS3231` y registro de eventos.

---

## Estado de Vertical Slices

| Slice | Issue | Funcionalidad | Estado | Documentación |
| :--- | :---: | :--- | :---: | :--- |
| **`SLICE-01`** | `#1` | Detección no bloqueante de corte/retorno `220V` (`500 ms` debounce + `3000 ms` estabilización) y respaldo híbrido (`UR18650ZT` + `TP4056` + `MT3608` + `1N5819`/`1N5822`) | Documentado | [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md) |
| **`SLICE-02`** | `#2` | Fotoperíodo diario con `RTC DS3231` (I2C, default `10:00–18:00`) y decisión automática post-corte (`LAMP_WHITE` diurno / `LAMP_OFF` nocturno) | Documentado | [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md) |
| **`SLICE-03`** | `#3` | Secuenciador IR TX no bloqueante (`KY-005` + `2N2222`, pulsos `250 ms`) y clonador IR RX (`KY-022`) para `ON`, `WHITE` y `OFF` | Documentado | [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md) |
| **`SLICE-04`** | `#4` | Cola circular offline *Store-and-Forward* (`16` eventos en RAM con timestamp RTC), comandos Telegram/Serial y cámara Android sin SIM (*IP Webcam* + Hotspot) | Documentado | [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md) |
| **`SLICE-05`** | `#5` | Dosificador de emergencia por tolva calibrada en ms (`50..2000 ms`, bloqueo en batería, cooldown `60 s`) y códigos IR 24 teclas pre-cargados (`8050SS` + `1N4007`) | Documentado | [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md) |

### Resumen de Pines (`SLICE-01` a `SLICE-05`)

| Señal | ESP32-C3 SuperMini | ESP32 Clásico | Arduino Nano |
| :--- | :---: | :---: | :---: |
| `PIN_FEEDER` (Servo 3 cables / Motor DC + `8050SS`, `+5V` antes de `D1`) | `GPIO 1` | `GPIO 25` | `D5` |
| `PIN_MAINS_SENSE` (Divisor `10kΩ/20kΩ` antes de `D1`) | `GPIO 3` | `GPIO 34` | `D2` |
| `RTC DS3231` — I2C (`SDA` / `SCL`) | `GPIO 6` / `GPIO 7` | `GPIO 21` / `GPIO 22` | `A4` / `A5` |
| Emisor IR (`KY-005` / LED IR reciclado + `2N2222` o `8050SS`) | `GPIO 4` | `GPIO 4` | `D3` |
| Receptor IR `KY-022` (`VS1838B` `38 kHz`) | `GPIO 5` | `GPIO 15` | `D4` |

- **Etapa de respaldo:** Celda `UR18650ZT` conectada en `B+`/`B-` del `TP4056` (6 pines) $\rightarrow$ salida `OUT+`/`OUT-` hacia el elevador `MT3608` (calibrado a `4.8V`) $\rightarrow$ diodo `D2` (`1N5819`/`1N5822` o `1N4007`) unido al cátodo de `D1` (`5V` USB) en el pin `5V` de la placa.
- **Alimentador (`SLICE-05`) y protecciones:** Alimentación de potencia tomada **antes del diodo `D1` (`1N4007`)** para no descargar la `UR18650ZT`; rango `50..2000 ms`, bloqueo automático con `onBatteryBackup` y cooldown de `60 s`. Códigos IR pre-cargados de 24 teclas: `ON=0xF7C03F`, `WHITE=0xF7E01F`, `OFF=0xF740BF`.
- **Comandos Telegram / Serie (`115200 baudios`):** `/estado`, `/cola`, `/foto`, `/luz_blanca`, `/luz_off`, `/auto`, `/horario HH:MM HH:MM`, `/clonar_ir`, `/net_on`, `/net_off`. Ver detalle completo en [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md).

---

## Arquitectura Multiagente del Repositorio

Este proyecto sigue una arquitectura **Multiagente con Aislamiento de Contexto**, **Vertical Slicing** y **Conventional Commits** con selección de hardware en **Mercado Libre Argentina**:

- [`AGENTS.md`](AGENTS.md): Orquestador global, protocolo de ahorro de tokens, reglas de Vertical Slicing y Conventional Commits.
- [`docs/hardware/bom-mercadolibre.md`](docs/hardware/bom-mercadolibre.md): Bill of Materials y esquemas eléctricos.
- [`docs/plans/PLAN-001-base.md`](docs/plans/PLAN-001-base.md): Plan de trabajo por Vertical Slices.
- [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md): Manual de uso, conexionado y funcionalidades implementadas.
- [`.agents/agents/hardware.md`](.agents/agents/hardware.md) · [`planificador.md`](.agents/agents/planificador.md) · [`gestor-issues.md`](.agents/agents/gestor-issues.md) · [`implementador.md`](.agents/agents/implementador.md) · [`tester.md`](.agents/agents/tester.md) · [`documentador.md`](.agents/agents/documentador.md)

