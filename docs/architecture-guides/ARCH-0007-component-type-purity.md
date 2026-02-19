# ARCH-0007: Component Type Purity

* **RFC Reference:** [RFC-0007 — Component Type Purity](../rfcs/02-implemented/0007-component-type-purity.md)
* **Implementation Date:** 2026-02-19
* **Status:** Active

---

## 1. High-Level Mental Model

> `components.hpp` is the schema for the entire ECS world — every system that touches
> data includes it. This RFC establishes the rule that **components must not include
> engine-library headers**: no `<raylib.h>`, no Jolt math types, no GPU handles.
> Components store *pure data*; systems own the conversion to/from external APIs at
> their boundary.

* **Core Responsibility:** Keep `components.hpp` free of Raylib and Jolt math types
  so that any translation unit — including headless test binaries — can include it
  without linking the full engine stack.
* **Pipeline Phase:** Not a runtime system; this is a structural constraint on
  `src/components.hpp` enforced at compile time.
* **Key Constraints:** `components.hpp` may include Jolt headers **only** for
  `RigidBodyHandle` and `CharacterHandle`, which hold opaque runtime physics
  handles. All other data uses ECS/standard types. This exception is temporary and
  will be revisited in a future ownership-model RFC.

---

## 2. The Grand Tour

| File | Role |
|------|------|
| `src/components.hpp` | The anchor. All three changes live here — new types, stripped includes, updated structs. |
| `src/systems/renderer.cpp` | Consumer of `Color4` → adds `to_raylib(Color4)` helper and builds `Camera3D` locally. |
| `src/systems/camera.cpp` | Writer of `MainCamera::view_forward/view_right`; uses local `to_v3`/`from_v3` helpers for Raylib vector ops. |
| `src/systems/character_input.cpp` | Reader of view directions — now reads from `MainCamera` resource instead of `PlayerInput`. |

### Lifecycle vs. Per-Frame Work

There is no `Register` hook or special lifecycle for this RFC — the changes are
purely structural. The runtime effects are:

**`CameraSystem::Update`** now:
1. Works with `ecs::Vec3` fields (`lerp_pos`, `lerp_target`, `smoothed_vel`) rather than `Vector3`/`JPH::Vec3`.
2. Builds a transient local `Camera3D` for the final view direction computation,
   then discards it — no `Camera3D` is stored in the component.
3. Writes the computed `view_forward` and `view_right` to `MainCamera` (the resource)
   instead of to `PlayerInput`.

**`RenderSystem::Update`** now:
1. Reads `cam->lerp_pos` / `cam->lerp_target` and constructs a `Camera3D` on the
   stack at draw time. Two lines, negligible cost.
2. Calls `to_raylib(mesh.color)` inline at draw time to convert `Color4` → Raylib
   `Color` for each drawn mesh.
3. Switches on `ShapeType` (enum class) rather than an integer.

**`CharacterInputSystem::Update`** now:
1. Reads view directions from `world.resource<MainCamera>()` at the top of `Update`,
   then uses them in the per-entity loop.

---

## 3. Components & Data Ownership

### New types introduced

| Type | Location | Purpose |
|------|----------|---------|
| `enum class ShapeType` | `components.hpp` | Replaces `int shape_type = 0` magic constants in `MeshRenderer`. Values: `Box`, `Sphere`, `Capsule`. |
| `struct Color4` | `components.hpp` | RGBA colour as four `[0, 1]` floats. Engine-independent; converted to Raylib `Color` at draw time only. |
| `namespace Colors` | `components.hpp` | Named constants (`Colors::Gray`, `Colors::Maroon`, etc.) matching the Raylib palette used in the scene. |

### Modified components

**`MeshRenderer`** — before/after:
```cpp
// Before
struct MeshRenderer {
    int   shape_type = 0;   // magic: 0=Box, 1=Sphere, 2=Capsule
    Color color      = WHITE; // Raylib type
    ...
};

// After
struct MeshRenderer {
    ShapeType shape_type = ShapeType::Box;
    Color4    color      = Colors::White;
    ...
};
```

**`MainCamera`** — fields changed:
```cpp
// Before                          // After
Vector3   lerp_pos    = ...        ecs::Vec3 lerp_pos    = ...
Vector3   lerp_target = ...        ecs::Vec3 lerp_target = ...
JPH::Vec3 smoothed_vel = ...       ecs::Vec3 smoothed_vel = ...
Camera3D  raylib_camera = {0};     // removed — computed locally in RenderSystem
                                   ecs::Vec3 view_forward = {0, 0, 1}; // new
                                   ecs::Vec3 view_right   = {1, 0, 0}; // new
```

**`PlayerInput`** — fields removed:
```cpp
// Before (written by CameraSystem — coupling violation)
ecs::Vec3 view_forward = {0, 0, 1};
ecs::Vec3 view_right   = {1, 0, 0};
// After: gone. These now live in MainCamera.
```

### Include state of `components.hpp` after this RFC

```cpp
// Retained — required for RigidBodyHandle and CharacterHandle
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <memory>

// Retained — our ECS types
#include <ecs/ecs.hpp>
#include <ecs/modules/transform.hpp>

// Removed
// #include <raylib.h>  ← was needed for Color, Vector3, Camera3D
```

---

## 4. Data Flow

The key data flow change is the view direction path:

```
Before:                             After:

CameraSystem                        CameraSystem
  └─ computes fwd/right               └─ computes fwd/right
  └─ writes PlayerInput                 └─ writes MainCamera.view_forward
       .view_forward ────────────────────────────────────────────┐
       .view_right                     MainCamera.view_right  ───┤
                                                                  │
CharacterInputSystem                CharacterInputSystem          │
  └─ reads PlayerInput.view_*         └─ reads MainCamera.view_* ┘
```

`PlayerInput` is now purely hardware intent. `MainCamera` is the sole owner of
camera-derived directions.

The `Camera3D` construction path:

```
Before:                             After:

CameraSystem writes                 CameraSystem writes
  MainCamera.raylib_camera            MainCamera.lerp_pos (ecs::Vec3)
                                       MainCamera.lerp_target (ecs::Vec3)
        │                                        │
RenderSystem reads                  RenderSystem reads
  cam->raylib_camera                  cam->lerp_pos / cam->lerp_target
  (pre-built Camera3D)                builds Camera3D on stack (2 lines)
```

---

## 5. System Integration (The Social Map)

This RFC tightens the coupling rules rather than changing execution order.

**CameraSystem** is the only writer of `MainCamera::view_forward/view_right`.
It must still run before `CharacterInputSystem` — the ordering constraint from
RFC-0006 is unchanged; only the mechanism changed (resource field instead of
PlayerInput field).

**RenderSystem** and **CharacterInputSystem** are now cleaner consumers:
- `RenderSystem` is fully decoupled from `MainCamera`'s internal smoothing
  representation — it only needs the final position and target.
- `CharacterInputSystem` no longer has an implicit dependency on `PlayerInput`
  containing camera-derived data.

---

## 6. Trade-offs & Gotchas

**`MathBridge::ToRaylib` removed from `components.hpp`.**
The two Raylib-returning helpers (`ToRaylib(ecs::Vec3)`, `ToRaylib(ecs::Quat)`)
were defined in `MathBridge` inside `components.hpp`. Removing `<raylib.h>` required
removing them. In practice they were unused by any system (systems converted types
inline). If you need `ecs::Vec3 → Vector3` conversion in a consumer file, use the
`{v.x, v.y, v.z}` aggregate or define a local helper — do not re-add it to
`components.hpp`.

**`Camera3D` is rebuilt each frame in RenderSystem — intentionally.**
This is not a performance concern (six field assignments per frame). The alternative
— caching `Camera3D` in `MainCamera` — is the pattern we just removed. Do not
re-introduce it.

**`Colors::` namespace is not exhaustive.**
Only the colours used in the current scene are defined. Adding a new scene colour
that maps to a Raylib macro requires adding a constant to the `Colors::` namespace
in `components.hpp`. The compiler will catch the omission (no implicit `Color →
Color4` conversion exists).

**Jolt handles remain in `components.hpp`.**
`RigidBodyHandle` holds a `JPH::BodyID` and `CharacterHandle` holds a
`shared_ptr<JPH::CharacterVirtual>`. These still require Jolt headers in
`components.hpp`. The headless test target therefore still cannot include
`components.hpp` without Jolt. This is the remaining blocker for
`CharacterStateSystem::apply_state` unit tests, and is earmarked for a future
handle-ownership RFC.

**`smoothed_vel` arithmetic uses a local Jolt conversion in `camera.cpp`.**
Because `cam.smoothed_vel` is now `ecs::Vec3` but the velocity value comes from
a Jolt `CharacterVirtual`, `camera.cpp` round-trips through `JPH::Vec3` locally
for the smoothing arithmetic (`LengthSq`, `Normalized`). This is contained inside
one block of `camera.cpp` and does not bleed into the component.

---

## 7. Related Resources

* **RFC:** [`docs/rfcs/02-implemented/0007-component-type-purity.md`](../rfcs/02-implemented/0007-component-type-purity.md)
* **Components:** `src/components.hpp` — `Color4`, `Colors::`, `ShapeType`, `MainCamera`, `PlayerInput`
* **Tests:** `CharacterStateSystem::apply_state` unit tests pending handle-ownership RFC (see Gotchas above)
* **ARCH_STATE.md:** Section 2 (Component Definitions) reflects the updated `MainCamera` and `PlayerInput` shapes
