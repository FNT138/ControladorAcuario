# Proyecto Arduino — Esquema Multiagente (Vertical Slicing + Conventional Commits)

Plantilla de proyecto Arduino optimizada para desarrollo multiagente con bajo consumo de tokens, **Vertical Slicing**, **Conventional Commits** y selección de hardware basada en componentes genéricos de **Mercado Libre Argentina**.

## Estructura de los 6 Subagentes

- [`AGENTS.md`](AGENTS.md): Orquestador global, protocolo de ahorro de tokens, reglas de Vertical Slicing y Conventional Commits.
- [`.agents/agents/hardware.md`](.agents/agents/hardware.md): Define el BOM en [`docs/hardware/bom-mercadolibre.md`](docs/hardware/bom-mercadolibre.md) priorizando módulos genéricos, económicos y fáciles de conseguir en Mercado Libre Argentina.
- [`.agents/agents/planificador.md`](.agents/agents/planificador.md): Diseña cómo funciona el sistema ([`docs/architecture/`](docs/architecture)) y divide el plan en **Vertical Slices** ([`docs/plans/`](docs/plans)).
- [`.agents/agents/gestor-issues.md`](.agents/agents/gestor-issues.md): Crea, publica y gestiona las issues en GitHub por cada Vertical Slice o bug detectado usando la skill [`git-issues`](.agents/skills/git-issues/SKILL.md).
- [`.agents/agents/implementador.md`](.agents/agents/implementador.md): Desarrolla únicamente código C/C++ en [`src/`](src) e [`include/`](include) de un Vertical Slice a la vez con Conventional Commits (`feat`, `fix`, `refactor`).
- [`.agents/agents/tester.md`](.agents/agents/tester.md): Ejecuta únicamente la suite predefinida en [`test/`](test) sin escribir ni modificar código.
- [`.agents/agents/documentador.md`](.agents/agents/documentador.md): Documenta incrementalmente las funcionalidades y el uso de cada Vertical Slice aprobado en [`docs/uso/`](docs/uso) y `README.md` (`docs(<scope>): ...`).
