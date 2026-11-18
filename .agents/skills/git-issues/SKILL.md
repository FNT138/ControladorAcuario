---
name: git-issues
description: >-
  Genera y publica issues de GitHub (features, tareas técnicas o bugs) a partir de los planes de trabajo
  del agente planificador (docs/plans/*.md) o de los reportes de fallos del agente tester usando GitHub CLI (gh).
  Activar cuando el usuario pida crear issues para git/GitHub desde un plan o desde un error de testeo.
---

# Skill: Generador de Issues para Git / GitHub (`git-issues`)

Esta skill convierte los planes de trabajo de `docs/plans/` o los reportes `FAIL` del `tester` en issues estructuradas en GitHub consumiendo la menor cantidad posible de tokens.

---

## 1. Prerrequisitos Rápidos

1. Verificar que `gh` esté disponible y autenticado:
   ```powershell
   gh auth status
   ```
   *(En Windows, si `gh` recién se instaló, usar `"C:\Program Files\GitHub CLI\gh.exe"` si aún no refrescó el `PATH`).*
2. Detectar el repositorio remoto (`ORG/REPO`):
   ```powershell
   git remote get-url origin
   ```
   > **Regla:** Pasar siempre `-R ORG/REPO` explícitamente a cada comando `gh issue`.

---

## 2. Modo A: Generar Issues desde un Plan (`docs/plans/PLAN-*.md`)

Cuando el `planificador` termina un plan de trabajo y el usuario pide pasar el plan a issues:

1. Leer **únicamente** el archivo `docs/plans/PLAN-<nombre>.md` correspondiente.
2. Por cada paso o módulo de implementación del plan, crear una issue atómica ejecutando:

```powershell
gh issue create -R ORG/REPO `
  --title "[FEAT] <Título corto de la tarea>" `
  --label "enhancement" `
  --body "## Objetivo (Planificador)`n<Resumen en 2 líneas>`n`n## Archivos a Modificar (Implementador)`n- `include/...``n- `src/...``n`n## Contratos / Firmas`n```cpp`n<prototipos clave>`n````n`n## Criterios de Aceptación (Tester)`n- [ ] Pasa `pio test -e native``n- [ ] Compila en placa (`pio run -e uno`)"
```

---

## 3. Modo B: Generar Issue de Bug desde un Reporte `FAIL` del Tester

Cuando el `tester` detecta un fallo en la suite predefinida y el usuario pide registrar el bug en GitHub:

1. Tomar el bloque compacto de salida del `tester` (sin leer logs crudos completos).
2. Ejecutar:

```powershell
gh issue create -R ORG/REPO `
  --title "[BUG] Fallo en test <nombre_test> (<archivo>:<línea>)" `
  --label "bug" `
  --body "## Dictamen del Tester`n- **Test fallido:** `<nombre_test>``n- **Ubicación:** `<archivo>:<línea>``n- **Esperado vs Obtenido:** Esperaba `<A>`, recibió `<B>``n- **Uso de Memoria:** RAM: `<X>%` | Flash: `<Y>%``n`n## Causa Probable`n<1 línea de diagnóstico técnico para el implementador>"
```

---

## 4. Modo Offline (Si el repositorio aún no tiene `remote` en GitHub)

Si el proyecto local todavía no tiene configurado un remoto en GitHub (`git remote` vacío):
1. Guardar las issues generadas como borradores listos para publicar en `docs/plans/issues-pendientes.md` o ejecutar el script [`scripts/publish_issues.ps1`](./scripts/publish_issues.ps1) una vez conectado el repositorio.

---

## 5. Regla de Economía de Tokens
- **No listar ni leer issues antiguas** salvo que el usuario pida verificar duplicados.
- **Devolver al chat únicamente la tabla resumen** con el número de issue (`#N`), título y URL devuelta por `gh issue create`.
