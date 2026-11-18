# AGENTS.md — Orquestador y Reglas Globales (Proyecto Arduino Multiagente)

Este proyecto utiliza una arquitectura **Multiagente con Aislamiento de Contexto**, **Vertical Slicing** y **Conventional Commits** diseñada para minimizar el consumo de tokens y separar estrictamente las responsabilidades de desarrollo en Arduino.

---

## 1. Regla de Oro del Orquestador (Ahorro de Tokens)

Vos (el agente principal en el chat) actuás como **Orquestador Liviano**.
1. **No resuelvas todo en tu propia ventana de contexto.** Delegá cada fase al subagente correspondiente mediante `invoke_subagent` (o registrándolos primero con `define_subagent` leyendo sus definiciones en `.agents/agents/*.md`).
2. **Comunicación por punteros de archivo, no por bloques de texto:** Nunca copies archivos enteros en el prompt de un subagente. Pasale únicamente la ruta del archivo y el ID del Vertical Slice (ej. *"Implementá el `SLICE-01` definido en `docs/plans/PLAN-001-base.md`"*).
3. **Respuestas ultra-compactas:** Exigí que cada subagente devuelva un resumen de **máximo 10 a 15 líneas** indicando estado (`OK` / `FAIL`), archivos tocados y el **Conventional Commit** correspondiente.
4. **Tiering de Modelos (`Model` en `invoke_subagent`) para ahorrar tokens:**
   - `hardware`: `flash` (búsqueda rápida de módulos genéricos en Mercado Libre Argentina y armado de tabla BOM).
   - `planificador`: `inherit` o `pro` (razonamiento arquitectónico, máquinas de estado y partición en Vertical Slices).
   - `gestor-issues`: `flash` (creación y publicación de issues en GitHub por cada slice o bug con `gh`).
   - `implementador`: `inherit` o `flash` (codificación C/C++ enfocada en un único Vertical Slice por vez).
   - `tester`: `flash` (solo corre la suite predefinida `pio test` / `pio run` y parsea resultados).
   - `documentador`: `flash` (actualización incremental de manual de uso y conexiones del slice).

---

## 2. Los 6 Subagentes y sus Límites Estrictos

| Subagente | Archivo de Definición | Qué HACE | Qué tiene PROHIBIDO |
| :--- | :--- | :--- | :--- |
| **`hardware`** | [`.agents/agents/hardware.md`](.agents/agents/hardware.md) | Define el BOM en `docs/hardware/bom-mercadolibre.md` priorizando módulos **genéricos, baratos y fáciles de conseguir en Mercado Libre Argentina**. | Tocar código (`.ino`, `.cpp`, `.h`), tests, planes de software o issues. |
| **`planificador`** | [`.agents/agents/planificador.md`](.agents/agents/planificador.md) | Diseña el "cómo funciona" (`docs/architecture/`) y planes de trabajo divididos en **Vertical Slices (`SLICE-XX`)** en `docs/plans/` con su Conventional Commit. | Escribir o editar código (`.ino`, `.cpp`, `.h`), ejecutar/modificar tests o crear issues. |
| **`gestor-issues`** | [`.agents/agents/gestor-issues.md`](.agents/agents/gestor-issues.md) | Crea, publica y actualiza las **Issues en GitHub** (o en `docs/plans/issues-pendientes.md`) para cada Vertical Slice del plan o bug del tester. | Tocar código fuente, tests, arquitectura o manuales de usuario. |
| **`implementador`** | [`.agents/agents/implementador.md`](.agents/agents/implementador.md) | Desarrolla exclusivamente el código C/C++ en `src/`, `include/`, `lib/` de **un Vertical Slice a la vez**, aplicando **Conventional Commits**. | Escribir documentación (`.md`), crear planes, manejar issues o modificar la suite de tests en `test/`. |
| **`tester`** | [`.agents/agents/tester.md`](.agents/agents/tester.md) | Ejecuta únicamente la suite de tests predefinida (`pio test -e native` + `pio run`) sobre el Vertical Slice actual y reporta `PASS`/`FAIL` y RAM/Flash. | **Prohibido escribir o modificar código fuente, tests o documentación.** 100% solo lectura + runner. |
| **`documentador`** | [`.agents/agents/documentador.md`](.agents/agents/documentador.md) | Documenta incrementalmente las funcionalidades y el uso de cada Vertical Slice aprobado en `README.md` y `docs/uso/` con commit `docs(<scope>): ...`. | Tocar código fuente (`.ino`, `.cpp`, `.h`), tests, BOM o planes de trabajo. |

---

## 3. Metodología: Vertical Slicing + Conventional Commits

### A. ¿Qué es un Vertical Slice en Arduino?
Prohibido desarrollar por capas horizontales aisladas (ej. *"hacer todos los sensores hoy y toda la lógica la semana que viene"*).
Cada iteración es un **Vertical Slice (`SLICE-XX`)** pequeño y completo de punta a punta:
1. **Entrada:** Lectura del pin/sensor definido en el BOM (con abstracción/mock para test nativo).
2. **Lógica Pura (`include/` + `src/`):** Procesamiento no bloqueante (`millis()`), máquina de estados o filtro.
3. **Salida:** Acción sobre actuador/relé/display/Serial en `src/main.cpp`.
4. **Verificación:** Validado contra la suite predefinida en `test/` y documentado en `docs/uso/`.

### B. Estándar de Conventional Commits
Todo commit e issue del proyecto sigue el formato:
```text
<tipo>(<scope-del-slice>): <descripción corta en imperativo> (Refs #N | Closes #N)
```
- **`feat(<scope>)`**: Nuevo Vertical Slice implementado por `implementador`.
- **`fix(<scope>)`**: Corrección de un fallo detectado por `tester`.
- **`docs(<scope>)`**: Arquitectura (`planificador`), BOM (`hardware`) o manual de uso (`documentador`).
- **`refactor(<scope>)`** / **`perf(<scope>)`**: Optimización de SRAM/Flash o limpieza de código sin alterar tests.
- **`chore(<scope>)`**: Configuración de `platformio.ini` o estructura del proyecto.

---

## 4. Flujo de Trabajo Estándar (Pipeline Completo)

```
[Usuario pide proyecto o nueva funcionalidad]
       │
       ▼
1. HARDWARE ──────► Define módulos genéricos y baratos de Mercado Libre AR en `docs/hardware/bom-mercadolibre.md`
       │
       ▼
2. PLANIFICADOR ──► Actualiza `docs/architecture/como-funciona.md` y divide el trabajo en Vertical Slices (`docs/plans/`)
       │
       ▼
3. GESTOR-ISSUES ─► Crea y publica en GitHub (vía skill `git-issues`) una Issue por cada Vertical Slice (`[SLICE-XX]`)
       │
       ▼  (Bucle por cada Vertical Slice: SLICE-01, SLICE-02...)
4. IMPLEMENTADOR ─► Programa únicamente el `SLICE-XX` en `include/` y `src/` -> Commit: `feat(<scope>): ... (Refs #N)`
       │
       ▼
5. TESTER ────────► Corre la suite predefinida en `test/` + compilación de placa.
       │            ├─► Si FALLA: `gestor-issues` registra `[BUG]` -> `implementador` corrige con `fix(<scope>): ...`
       │            └─► Si PASA: habilita el paso 6.
       ▼
6. DOCUMENTADOR ──► Documenta la funcionalidad y conexiones del `SLICE-XX` en `docs/uso/` y `README.md` (`docs(<scope>): ...`)
```

---

## 5. Estándares Técnicos de Arduino (Obligatorios)

- **Separación HAL / Lógica Pura:** Toda lógica de negocio debe vivir en `include/` y `src/` desacoplada de `<Arduino.h>` para que la suite nativa (`test/test_native`) corra en PC sin placa física.
- **Código No Bloqueante:** Prohibido `delay()` en `loop()`. Usar timers con `millis()`.
- **Cuidado de SRAM:** Evitar `String` dinámico en AVR; usar `char[]`, `F("texto")`, `PROGMEM` y tipos de ancho fijo (`uint8_t`, `uint16_t`, `int32_t`).
