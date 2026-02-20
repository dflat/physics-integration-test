# RFC-0008: Physics Handle Isolation

* **Status:** Implemented
* **Date:** February 2026
* **Depends on:** RFC-0007 (Component Type Purity — removes Raylib from components.hpp,
  leaving Jolt as the only remaining engine dependency)

## Summary

Extract `RigidBodyHandle`, `CharacterHandle`, and `MathBridge` from `components.hpp`
into a new `src/physics_handles.hpp` header. After this RFC, `components.hpp` has
no engine-library dependencies at all — only ECS and standard-library types — making
it (and `character_state.hpp`) compilable in the headless test target. This unblocks
the `CharacterStateSystem::apply_state` unit tests deferred since RFC-0006.

## Motivation

RFC-0007 removed Raylib from `components.hpp`. The remaining Jolt dependency is
three includes at the top of the file:

```cpp
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
```

These exist solely to define `RigidBodyHandle` (`JPH::BodyID`) and `CharacterHandle`
(`std::shared_ptr<JPH::CharacterVirtual>`). They are runtime-handle types — opaque
links into the physics simulation managed by `PhysicsSystem` and
`CharacterMotorSystem`. They are not data components.

The test target (`unit_tests`) links only `ecs` and `Catch2` — no Jolt. As long as
`components.hpp` includes Jolt headers, the test target cannot include it, and the
pure `CharacterStateSystem::apply_state` function (which has zero Jolt dependencies
in its signature or body) cannot be exercised from tests.

## Design

### New file: `src/physics_handles.hpp`

All Jolt-dependent definitions move here:

```cpp
#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <memory>
#include <ecs/ecs.hpp>

namespace MathBridge { ... }   // ecs::Vec3 <-> JPH::Vec3 conversions

struct RigidBodyHandle { JPH::BodyID id; };
struct CharacterHandle { std::shared_ptr<JPH::CharacterVirtual> character; };
```

### Modified: `src/components.hpp`

- Remove the three Jolt includes and `<memory>`
- Remove `MathBridge`
- Remove `RigidBodyHandle` and `CharacterHandle`

The five engine systems that use these types add `#include "../physics_handles.hpp"`:
`camera.cpp`, `character_input.cpp`, `character_state.cpp`, `character_motor.cpp`,
`physics.cpp`.

### Modified: `src/systems/character_state.hpp`

`apply_state` is moved inline into the header. Its body has no Jolt dependency —
it takes only `bool`, `float`, `const CharacterIntent&`, `CharacterState&`. Inlining
it means the test target can call it by including only `character_state.hpp` →
`components.hpp`, with zero Jolt involvement.

### New tests: `tests/logic_tests.cpp`

Remove the deferral comment and add `TEST_CASE` blocks covering:
- Grounded reset (mode, jump_count, air_time all cleared)
- Airborne air_time accumulation
- First jump impulse (12.0 m/s) and jump_count bookkeeping
- Double jump impulse (10.0 m/s)
- Jump exhaustion (jump_count == 2 → no impulse)
- Coyote jump (airborne with jump_count == 0)
- `jump_impulse` cleared at the top of every call (one-frame signal)

### Files Changed

| Action | Path | Reason |
|--------|------|--------|
| Create | `src/physics_handles.hpp` | New home for Jolt-dependent handle types and MathBridge |
| Modify | `src/components.hpp` | Remove Jolt includes, MathBridge, handle structs |
| Modify | `src/systems/character_state.hpp` | Move `apply_state` inline |
| Modify | `src/systems/character_state.cpp` | Remove `apply_state` body; add physics_handles include |
| Modify | `src/systems/camera.cpp` | Add physics_handles include |
| Modify | `src/systems/character_input.cpp` | Replace explicit Jolt includes with physics_handles |
| Modify | `src/systems/character_motor.cpp` | Add physics_handles include |
| Modify | `src/systems/physics.cpp` | Add physics_handles include |
| Modify | `tests/logic_tests.cpp` | Add apply_state test cases |

`CMakeLists.txt` requires no changes — the test target already links only `ecs` and
`Catch2`.

## Alternatives Considered

**Forward-declare `JPH::BodyID` and `JPH::CharacterVirtual`.** `JPH::BodyID` is a
value type (not a pointer), so it cannot be forward-declared. This approach is not
viable.

**Store handles as `uint32_t` / `void*`.** Type-erasing the handles removes
compiler-enforced type safety. The header-split approach preserves types and requires
no casting at call sites.

**Move handles to a `components_runtime.hpp` that `components.hpp` includes
conditionally.** Conditional includes are fragile and hard to reason about. A clean
split — `components.hpp` for data, `physics_handles.hpp` for runtime — is simpler.

## Testing

- `cmake --build build --target unit_tests && cd build && ctest` must pass with the
  new `apply_state` test cases.
- The demo build must continue to compile and run with identical visual behaviour.
