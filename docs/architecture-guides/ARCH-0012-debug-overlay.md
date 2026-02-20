# ARCH-0012: Debug Overlay

* **RFC Reference:** [RFC-0012 — Debug Overlay](../rfcs/02-implemented/0012-debug-overlay.md)
* **Implementation Date:** 2026-02-20
* **Status:** Active

---

## 1. High-Level Mental Model

> Register once, inspect always. Any code with access to `DebugPanel` and the
> `World` can call `panel.watch(section, label, fn)` at startup. Every render
> frame `DebugSystem` calls each registered provider and draws the results.
> No system needs to be modified; no per-frame write coordination is required.

* **Core Responsibility:** Drive a toggleable on-screen text overlay from a
  registry of named `std::function<std::string()>` providers.
* **Pipeline Phase:** Render — runs after `RenderSystem` so the overlay draws
  on top of the 3D scene.
* **Key Constraint:** Providers are registered at startup and capture `World&`
  by reference. Both the `World` and the `DebugPanel` resource live for the
  entire program — the captures are always valid.

---

## 2. The Grand Tour

| File | Role |
|------|------|
| `src/debug_panel.hpp` | `DebugPanel` resource — Section/Row/Provider types, `watch()`, `sections()`. Zero engine deps. |
| `src/systems/debug.hpp` | `DebugSystem` declaration. |
| `src/systems/debug.cpp` | F3 toggle + provider dispatch + Raylib text rendering. |
| `src/main.cpp` | Provider registration (Engine + Character sections). |
| `tests/logic_tests.cpp` | 5 headless unit tests for `DebugPanel` (`[debug]`). |

### No `Register()`, no frame-scope clearing

`DebugSystem` has no `Register()` — no lifecycle hooks. The `DebugPanel` is
not frame-scoped: providers are called lazily by `DebugSystem` in the Render
phase, so there is nothing to clear or flush between frames.

---

## 3. Data Ownership

```
DebugPanel (World resource)
  visible: bool
  sections_: vector<Section>
    Section { title, rows: vector<Row> }
      Row { label, fn: function<string()> }
```

`DebugPanel` owns all registered providers. Providers are `std::function`
closures — they may capture `World&`, Raylib state, or any other value whose
lifetime exceeds the panel's (which is the program's entire run).

### `DebugPanel` vs `AssetResource` / `AudioResource`

| | `DebugPanel` | `AssetResource` / `AudioResource` |
|---|---|---|
| Content | Function registry | Handle registry |
| Written by | `watch()` at startup | `load()` at startup |
| Read by | `DebugSystem` (Render) | Other systems (Logic/Render) |
| Frame-scoped | No | No |
| Engine deps in header | None | Raylib (`Shader`, `Sound`) |

---

## 4. Data Flow

```
Startup:
  panel.watch("Engine",    "FPS",        []() { return std::to_string(GetFPS()); })
  panel.watch("Engine",    "Frame Time", []() { ... })
  panel.watch("Engine",    "Entities",   [&world]() { return std::to_string(world.count()); })
  panel.watch("Character", "Mode",       [&world]() { query CharacterState → string })
  panel.watch("Character", "Jump Count", [&world]() { query CharacterState → string })
  panel.watch("Character", "Air Time",   [&world]() { query CharacterState → string })

Render frame (DebugSystem::Update):
  if KEY_F3 pressed → toggle panel.visible
  if not visible → return
  for each Section in panel.sections():
    for each Row in section.rows:
      val = row.fn()          ← provider called here
      DrawText(label, val)
```

---

## 5. System Integration (The Social Map)

**`DebugSystem` has no upstream dependencies on other systems** — it queries
the `World` directly via captured lambdas. It does not import any system header.

**Adding a new row from any context:**
```cpp
panel.watch("Physics", "Bodies", [&world]() {
    // any query, any world.try_resource<T>(), any Raylib call
    return some_string;
});
```
No existing file changes required except the registration call site.

**Extension convention (future):** Systems may expose a static
`debug_register(DebugPanel&, ecs::World&)` method grouping their own rows.
Called from `main.cpp` during setup, this keeps debug row definitions close
to the system they describe while keeping debug code out of the `Update` path.

---

## 6. Rendering Layout

```
┌────────────────────────────┐   ← x=10, y=10
│  DEBUG               [F3] │   ← title row
├────────────────────────────┤   ← divider line per section
│  Engine                   │   ← section header (gold)
│    FPS          60        │   ← label (light gray) + value (white)
│    Frame Time   16 ms     │
│    Entities     15        │
├────────────────────────────┤
│  Character                │
│    Mode         Grounded  │
│    Jump Count   0         │
│    Air Time     0.00 s    │
└────────────────────────────┘
```

Panel width: 230 px. Label column: 112 px from content left. Font size 10 (rows)
/ 11 (section headers). Background: `{20, 20, 20, 210}`. Panel height is
computed dynamically from section and row count each frame.

---

## 7. Trade-offs & Gotchas

**Provider call order = registration order.** Sections appear in the order
`watch()` was first called for that section title. Rows within a section appear
in call order. There is no sorting or priority mechanism in v1.

**`each<T>` requires `(Entity, T&)` signature.** The ECS `world.each<T>`
always passes the entity as the first argument. Providers that query components
must include the unused `ecs::Entity` parameter in their inner lambda.

**`debug.cpp` is not in `unit_tests`.** Raylib rendering requires a window.
The `DebugPanel` logic (`watch`, `sections`) is unit-tested headlessly in the
`[debug]` Catch2 cases (25 total test cases, 73 assertions).

**Long value strings overflow the panel.** Values should be kept to ~15
characters. No wrapping is implemented in v1.

---

## 8. Related Resources

* **RFC:** [`docs/rfcs/02-implemented/0012-debug-overlay.md`](../rfcs/02-implemented/0012-debug-overlay.md)
* **DebugPanel:** `src/debug_panel.hpp`
* **DebugSystem:** `src/systems/debug.hpp`, `src/systems/debug.cpp`
* **Tests:** `tests/logic_tests.cpp` — `[debug]` test cases
* **ARCH_STATE.md:** Section 4 (System Responsibilities)
