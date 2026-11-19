# Controlador de Pecera a Distancia (`ControladorPeceraDistancia`)

Controlador no bloqueante multi-placa (**Arduino Nano V3**, **ESP32 Clásico** y **ESP32-C3 SuperMini**) para automatización de iluminación LED IR de pecera con respaldo de energía ante cortes de `220V`, reloj `RTC DS3231` y registro de eventos.

---

## Estado de Vertical Slices

| Slice | Issue | Funcionalidad | Estado | Documentación |
| :--- | :---: | :--- | :---: | :--- |
| **`SLICE-01`** | `#1` | Detección no bloqueante de corte/retorno `220V` (`500 ms` debounce + `3000 ms` estabilización) y respaldo híbrido (`UR18650ZT` + `TP4056` + `MT3608` + `1N5819`/`1N5822`) | Documentado | [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md) |

### Resumen de Pines de Energía y Detección 220V (`SLICE-01`)

| Señal | ESP32-C3 SuperMini | ESP32 Clásico | Arduino Nano |
| :--- | :---: | :---: | :---: |
| `PIN_MAINS_SENSE` (Divisor `10kΩ/20kΩ` antes de `D1`) | `GPIO 3` | `GPIO 34` | `D2` |

- **Etapa de respaldo:** Celda `UR18650ZT` conectada en `B+`/`B-` del `TP4056` (6 pines) $\rightarrow$ salida `OUT+`/`OUT-` hacia el elevador `MT3608` (calibrado a `4.8V`) $\rightarrow$ diodo Schottky `D2` (`1N5819`/`1N5822`) unido al cátodo de `D1` (`5V` USB) en el pin `5V` de la placa. Ver guía completa en [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md).

---

## Arquitectura Multiagente del Repositorio

Este proyecto sigue una arquitectura **Multiagente con Aislamiento de Contexto**, **Vertical Slicing** y **Conventional Commits** con selección de hardware en **Mercado Libre Argentina**:

- [`AGENTS.md`](AGENTS.md): Orquestador global, protocolo de ahorro de tokens, reglas de Vertical Slicing y Conventional Commits.
- [`docs/hardware/bom-mercadolibre.md`](docs/hardware/bom-mercadolibre.md): Bill of Materials y esquemas eléctricos.
- [`docs/plans/PLAN-001-base.md`](docs/plans/PLAN-001-base.md): Plan de trabajo por Vertical Slices.
- [`docs/uso/manual-de-uso.md`](docs/uso/manual-de-uso.md): Manual de uso, conexionado y funcionalidades implementadas.
- [`.agents/agents/hardware.md`](.agents/agents/hardware.md) · [`planificador.md`](.agents/agents/planificador.md) · [`gestor-issues.md`](.agents/agents/gestor-issues.md) · [`implementador.md`](.agents/agents/implementador.md) · [`tester.md`](.agents/agents/tester.md) · [`documentador.md`](.agents/agents/documentador.md)

