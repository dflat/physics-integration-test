# RFC-0007: Component Type Purity

* **Status:** Proposal
* **Date:** February 2026
* **Depends on:** RFC-0006 (introduces `CharacterInputSystem`, which drives one
  migration in this RFC)

## Summary

Remove Raylib and Jolt math types from data components and fix the bidirectional
coupling between `CameraSystem` and `PlayerInput`. After this RFC, `components.hpp`
has no `#include <raylib.h>` and no Jolt math types — only ECS/standard library
types — making components usable in headless test contexts.

## Motivation

`components.hpp` currently includes `<raylib.h>` and three Jolt headers. This has
two concrete consequences:

**1. Components can't be used in tests without a display context.**
Catch2 unit tests run headless. Any test that constructs a `MainCamera` or
`MeshRenderer` today implicitly links Raylib, which tries to initialise a window or
OpenGL context on some platforms. The math-utility tests in `tests/logic_tests.cpp`
work around this by extracting functions into `math_util.hpp` — the same principle
should apply to the components themselves.

**2. Component headers encode implementation details.**
`MainCamera::raylib_camera` is a `Camera3D` — a Raylib rendering struct — sitting
inside a gameplay component. Anything that includes `components.hpp` to check camera
orbit angles now transitively depends on the entire Raylib rendering API. Similarly,
`MainCamera::smoothed_vel` is a `JPH::Vec3`, and `MainCamera::lerp_pos/lerp_target`
are `Vector3` (Raylib). None of these types belong in a data component; they are
implementation details of the systems that consume or produce this data.

**3. `PlayerInput` is written by `CameraSystem`.**
`CameraSystem` currently writes `view_forward` and `view_right` into `PlayerInput`
so that `CharacterSystem` can read them. Writing into another system's output
component is a design smell: `PlayerInput` models hardware intent, not camera state.
The view directions belong in the camera's own data.

## Design

### Overview

Three targeted changes, each independently deployable:

1. **`MeshRenderer` — shape enum + pure color type.** Replace the `int shape_type`
   magic constant with an `enum class ShapeType` and replace the Raylib `Color` with
   a plain `Color4` struct.

2. **`MainCamera` — strip Raylib and Jolt types.** Replace `Vector3` fields with
   `ecs::Vec3`, replace `JPH::Vec3` with `ecs::Vec3`, remove `Camera3D
   raylib_camera` (computed at render time instead), and add `view_forward` /
   `view_right` as the authoritative output of `CameraSystem`.

3. **View direction decoupling.** Remove `view_forward` / `view_right` from
   `PlayerInput`. `CameraSystem` writes them to `MainCamera`. After RFC-0006,
   `CharacterInputSystem` reads them from `MainCamera` instead.

The runtime-handle components (`RigidBodyHandle`, `CharacterHandle`) retain their
Jolt types — they are not data components, they are opaque links into the simulation.
Their includes are acceptable and are addressed separately when the ownership model
is revisited.

### Change 1 — `MeshRenderer`

```cpp
// Before
struct MeshRenderer {
    int   shape_type   = 0; // 0=Box, 1=Sphere, 2=Capsule
    Color color        = WHITE;
    ecs::Vec3 scale_offset = {1,1,1};
};

// After
enum class ShapeType { Box, Sphere, Capsule };

struct Color4 {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

// Convenience constants matching the Raylib colours currently in use
namespace Colors {
    inline constexpr Color4 White   = {1.0f, 1.0f, 1.0f, 1.0f};
    inline constexpr Color4 Gray    = {0.5f, 0.5f, 0.5f, 1.0f};
    inline constexpr Color4 SkyBlue = {0.40f, 0.75f, 1.0f, 1.0f};
    inline constexpr Color4 Gold    = {1.0f, 0.80f, 0.0f, 1.0f};
    inline constexpr Color4 Green   = {0.0f, 0.90f, 0.27f, 1.0f};
    inline constexpr Color4 Red     = {0.90f, 0.16f, 0.22f, 1.0f};
}

struct MeshRenderer {
    ShapeType shape_type   = ShapeType::Box;
    Color4    color        = Colors::White;
    ecs::Vec3 scale_offset = {1, 1, 1};
};
```

`renderer.cpp` converts `Color4` → Raylib `Color` at draw time:
```cpp
// src/systems/renderer.cpp
inline Color ToRaylib(const Color4& c) {
    return Color {
        static_cast<unsigned char>(c.r * 255),
        static_cast<unsigned char>(c.g * 255),
        static_cast<unsigned char>(c.b * 255),
        static_cast<unsigned char>(c.a * 255),
    };
}
```

The switch on `int shape_type` in `RenderSystem::Update` becomes a switch on
`ShapeType`, removing the silent fallthrough for unknown values.

### Change 2 — `MainCamera`

```cpp
// Before
struct MainCamera {
    float     orbit_phi            = 0.0f;
    float     orbit_theta          = 0.6f;
    float     orbit_distance       = 25.0f;
    int       zoom_index           = 1;
    Vector3   lerp_pos             = {0, 10, 20};   // Raylib
    Vector3   lerp_target          = {0, 0, 0};     // Raylib
    JPH::Vec3 smoothed_vel         = JPH::Vec3::sZero(); // Jolt
    float     last_manual_move_time = 0.0f;
    bool      follow_mode          = false;
    Camera3D  raylib_camera        = {0};            // Raylib
};

// After
struct MainCamera {
    float     orbit_phi             = 0.0f;
    float     orbit_theta           = 0.6f;
    float     orbit_distance        = 25.0f;
    int       zoom_index            = 1;
    ecs::Vec3 lerp_pos              = {0, 10, 20};
    ecs::Vec3 lerp_target           = {0, 0, 0};
    ecs::Vec3 smoothed_vel          = {0, 0, 0};
    float     last_manual_move_time = 0.0f;
    bool      follow_mode           = false;

    // Outputs written by CameraSystem, read by CharacterInputSystem (RFC-0006)
    ecs::Vec3 view_forward          = {0, 0, 1};
    ecs::Vec3 view_right            = {1, 0, 0};
};
```

`Camera3D raylib_camera` is removed from the struct. `RenderSystem` computes a
local `Camera3D` at draw time from `orbit_phi`, `orbit_theta`, `orbit_distance`,
and `lerp_pos` / `lerp_target`:

```cpp
// src/systems/renderer.cpp  (inside RenderSystem::Update)
auto& cam = world.resource<MainCamera>();
Camera3D raylib_cam = {};
raylib_cam.position   = MathBridge::ToRaylib(cam.lerp_pos);
raylib_cam.target     = MathBridge::ToRaylib(cam.lerp_target);
raylib_cam.up         = {0, 1, 0};
raylib_cam.fovy       = 60.0f;
raylib_cam.projection = CAMERA_PERSPECTIVE;
```

`CameraSystem` similarly constructs the Raylib camera internally where needed for
any intermediate math, and writes `view_forward` / `view_right` to the resource.

### Change 3 — View Direction Decoupling

```cpp
// Before — PlayerInput owned the camera's output
struct PlayerInput {
    ecs::Vec2 move_input   = {0, 0};
    ecs::Vec2 look_input   = {0, 0};
    bool      jump         = false;
    bool      plant_platform = false;
    float     trigger_val  = 0.0f;
    ecs::Vec3 view_forward = {0, 0, 1};  // written by CameraSystem ← coupling
    ecs::Vec3 view_right   = {1, 0, 0};  // written by CameraSystem ← coupling
};

// After — PlayerInput is pure hardware intent
struct PlayerInput {
    ecs::Vec2 move_input     = {0, 0};
    ecs::Vec2 look_input     = {0, 0};
    bool      jump           = false;
    bool      plant_platform = false;
    float     trigger_val    = 0.0f;
};
```

`CameraSystem::Update` stops writing to `PlayerInput` and instead writes
`view_forward` / `view_right` to the `MainCamera` resource (already owns orbit
state).

After RFC-0006, `CharacterInputSystem::Update` reads view directions from
`world.resource<MainCamera>()` rather than from the `PlayerInput` component.

### Headers After This RFC

```cpp
// src/components.hpp — includes after RFC-0007
#include <ecs/ecs.hpp>
#include <ecs/modules/transform.hpp>
#include <Jolt/Physics/Body/BodyID.h>           // RigidBodyHandle
#include <Jolt/Physics/Character/CharacterVirtual.h> // CharacterHandle
#include <memory>                                // shared_ptr in CharacterHandle
```

`<raylib.h>` is gone entirely. The two remaining Jolt includes are runtime-handle
dependencies; they are addressed when the character-handle ownership model is
revisited.

### Files Changed

| Action | Path | Reason |
|--------|------|--------|
| Modify | `src/components.hpp` | All three changes above |
| Modify | `src/systems/renderer.cpp` | `ShapeType` switch, `Color4→Color` helper, compute `Camera3D` locally |
| Modify | `src/systems/camera.cpp` | Write `view_forward`/`view_right` to `MainCamera`; stop writing to `PlayerInput` |
| Modify | `src/systems/character_input.cpp` | Read view dirs from `MainCamera` (requires RFC-0006) |
| Modify | `src/main.cpp` | Update `MeshRenderer` literals (Raylib colour macros → `Colors::*`) |
| Modify | `tests/logic_tests.cpp` | Can now construct `MainCamera`/`MeshRenderer` without Raylib |

### Migration

All call sites that construct `MeshRenderer` with a Raylib colour literal need
updating:

```cpp
// Before
world.add(e, MeshRenderer{ 0, GRAY, {1,1,1} });

// After
world.add(e, MeshRenderer{ ShapeType::Box, Colors::Gray, {1,1,1} });
```

All call sites that read `PlayerInput::view_forward` / `view_right` must switch to
reading from `world.resource<MainCamera>()`. After RFC-0006 there is exactly one
such call site: `CharacterInputSystem::Update`.

## Alternatives Considered

**Use `uint32_t` RGBA instead of `Color4`.** Simpler storage but loses float
precision for HDR or shader-driven colouring later. `Color4` (4 floats) is the
idiomatic choice for a data component.

**Leave `Camera3D` in `MainCamera` and only fix the includes.** Forward-declaring
Raylib structs is brittle; the real fix is not storing a rendering aggregate in a
data component. Computing `Camera3D` at render time is zero-overhead and is the
correct boundary.

**Move view dirs to a new `CameraOutput` resource.** An extra resource for two
fields is unnecessary indirection. `MainCamera` is already a `CameraSystem`-owned
resource; adding `view_forward`/`view_right` as output fields is the natural fit.

**Fix `CharacterHandle` ownership in this RFC.** The `shared_ptr<CharacterVirtual>`
and `JPH::BodyID` are runtime-handle concerns, not data-component concerns. They
belong in a dedicated ownership-model RFC where the full pool/arena design can be
considered.

## Testing

- **Build-time:** `components.hpp` must compile in a translation unit that does not
  include `<raylib.h>`. Add a minimal `tests/components_headless.cpp` that
  constructs `MainCamera`, `MeshRenderer`, and `PlayerInput` and verify the test
  target builds and links without Raylib.
- **Functional:** All visual output (shapes, colours, camera follow, orbit) must
  be identical before and after the change.
- **Existing tests:** `tests/logic_tests.cpp` must continue to pass unchanged.

## Risks & Open Questions

- **`Colors::*` completeness.** The constants in `Colors::` namespace cover only
  the colours currently used in `main.cpp`. Any future call site adding a new
  Raylib colour macro needs a matching constant; this should be caught at compile
  time (no implicit `Color` → `Color4` conversion).
- **`Camera3D` recomputation cost.** Computing a `Camera3D` each render frame from
  `lerp_pos` / `lerp_target` is six struct-field assignments — negligible.
- **Dependency on RFC-0006.** Change 3 (view direction decoupling) only fully
  cleans up `CharacterInputSystem` if RFC-0006 has landed. If implemented
  independently, `CharacterSystem` still reads `PlayerInput` and must be updated to
  read from `MainCamera` at that point.
