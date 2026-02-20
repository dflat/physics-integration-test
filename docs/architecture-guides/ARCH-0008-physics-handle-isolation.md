# ARCH-0008: Physics Handle Isolation

* **RFC Reference:** [RFC-0008 — Physics Handle Isolation](../rfcs/02-implemented/0008-physics-handle-isolation.md)
* **Implementation Date:** 2026-02-19
* **Status:** Active

---

## 1. High-Level Mental Model

> `components.hpp` is now a **zero-dependency schema** — it includes nothing
> except the ECS library and standard headers. `physics_handles.hpp` is the
> **Jolt annex**: it lives alongside components but is only included by the five
> engine systems that directly touch the physics simulation. The test target
> never sees a Jolt header.

* **Core Responsibility:** Isolate Jolt runtime handle types and `MathBridge`
  from the shared component schema so that `components.hpp` can be included
  in a headless context with no link-time physics dependency.
* **Pipeline Phase:** Not a runtime system — a structural constraint on which
  files include which headers.
* **Key Result:** `CharacterStateSystem::apply_state` is now exercised by unit
  tests without linking Jolt or Raylib. Phase 1 (Foundation Hardening) is
  complete.

---

## 2. The Grand Tour

| File | Role |
|------|------|
| `src/physics_handles.hpp` | New header. Contains `MathBridge`, `RigidBodyHandle`, `CharacterHandle`. All Jolt includes centralised here. |
| `src/components.hpp` | Down to two includes (`<ecs/ecs.hpp>`, `<ecs/modules/transform.hpp>`). Fully engine-agnostic. |
| `src/systems/character_state.hpp` | `apply_state` moved inline. Header now compilable — and callable — without linking Jolt. |
| `src/systems/character_state.cpp` | `apply_state` body removed. Adds `#include "../physics_handles.hpp"` for `CharacterHandle` used in `Update`. |
| Engine systems (×5) | `camera.cpp`, `character_input.cpp`, `character_motor.cpp`, `physics.cpp`, `renderer.cpp` all add `#include "../physics_handles.hpp"`. |
| `tests/logic_tests.cpp` | Deferred comment replaced by seven `apply_state` test cases. No Jolt in the test target. |

### Lifecycle vs. Per-Frame Work

This RFC has no lifecycle hooks and no per-frame behavior. All changes are
structural (header layout) and behavioral only in tests.

---

## 3. Components & Data Ownership

### The Two-Header Rule

```
src/
├── components.hpp        ← include freely; zero engine deps
│     ShapeType, Color4, Colors::, BodyType, BoxCollider, SphereCollider,
│     RigidBodyConfig, CharacterControllerConfig,
│     MeshRenderer, PlayerInput, MainCamera,
│     CharacterIntent, CharacterState, PlayerState,
│     PlayerTag, WorldTag
│
└── physics_handles.hpp   ← include only in engine systems; pulls in Jolt
      MathBridge (ecs::Vec3 ↔ JPH::Vec3 conversions)
      RigidBodyHandle { JPH::BodyID id; }
      CharacterHandle { shared_ptr<JPH::CharacterVirtual> character; }
```

**Rule of thumb:** if a file needs to query `CharacterHandle` or call
`MathBridge::ToJolt`, it must include `physics_handles.hpp`. If it only
reads gameplay state (`CharacterIntent`, `MainCamera`, etc.), `components.hpp`
is sufficient.

### Why `apply_state` is inline

`apply_state` takes only `bool`, `float`, `CharacterIntent`, `CharacterState`
— no Jolt type appears in its signature or body. Moving it inline into
`character_state.hpp` allows the test target to call it via:

```cpp
#include "../src/systems/character_state.hpp"
// → includes ../src/components.hpp (Jolt-free)
// → apply_state definition is here, inline
CharacterStateSystem::apply_state(on_ground, dt, intent, state);
```

The test target links only `ecs` and `Catch2`. No Jolt. No Raylib.

---

## 4. Data Flow

No runtime data flow changes — this RFC is purely structural. The component
ownership table and system ordering from RFC-0006/0007 are unchanged.

The only change visible at runtime is that `CharacterStateSystem::Update` now
calls the inline `apply_state` defined in the header rather than the
out-of-line definition that was in the `.cpp`. Behavior is identical.

---

## 5. System Integration (The Social Map)

**Which systems include `physics_handles.hpp`:**

| System | Reason |
|--------|--------|
| `camera.cpp` | Queries `CharacterHandle` for velocity smoothing; uses `MathBridge` |
| `character_input.cpp` | Uses `MathBridge::ToJolt`/`FromJolt` for move vector projection |
| `character_state.cpp` | Queries `CharacterHandle` in `Update` for ground state |
| `character_motor.cpp` | Creates and steps `CharacterHandle`; heavy `MathBridge` use |
| `physics.cpp` | Creates, stores, destroys `RigidBodyHandle`; uses `MathBridge` |
| `renderer.cpp` | Reads `CharacterHandle` to draw the player orientation gizmo |

**Systems that do NOT include `physics_handles.hpp`:**

`builder.cpp`, `input_gather.cpp`, `player_input.cpp` — these deal only with
gameplay components and have no physics handle access.

`main.cpp` — creates entities with authoring configs (`RigidBodyConfig`,
`CharacterControllerConfig`). The handles are added by systems via lifecycle
callbacks; `main.cpp` never references them directly.

---

## 6. Trade-offs & Gotchas

**`apply_state` inline is not a performance concern.**
Seven `TEST_CASE`s call it with trivial inputs. In the demo it is called once
per character per frame. Inlining a 20-line state machine is standard practice.

**`character_state.cpp` still includes Jolt** (via `physics_handles.hpp`).
The `.cpp` compiles into the demo target, which links Jolt. The test target
does not compile `character_state.cpp` at all — it calls the inline header
definition. This is the clean separation.

**`MathBridge` lives in `physics_handles.hpp`, not `math_util.hpp`.**
`math_util.hpp` contains pure float utilities (angle normalization, etc.) and
has no Jolt dependency. `MathBridge` converts between `ecs::Vec3` and
`JPH::Vec3`, which requires `<Jolt/Jolt.h>`. Keeping them separate maintains
the headless boundary.

**Adding a new system that reads gameplay state only** (e.g., an animation
system that reads `CharacterState`) should include only `components.hpp`.
Only add `physics_handles.hpp` if you genuinely need a handle type or
`MathBridge`.

**Phase 1 is complete.** RFC-0006 through RFC-0008 have delivered:
1. Three-system character pipeline (RFC-0006)
2. Engine-type-free components (RFC-0007)
3. Headless-compilable schema + `apply_state` tests (RFC-0008)

---

## 7. Related Resources

* **RFC:** [`docs/rfcs/02-implemented/0008-physics-handle-isolation.md`](../rfcs/02-implemented/0008-physics-handle-isolation.md)
* **Headers:** `src/components.hpp`, `src/physics_handles.hpp`
* **Tests:** `tests/logic_tests.cpp` — `apply_state` test cases
* **ARCH_STATE.md:** Section 2 (Component Definitions) — two-header rule
