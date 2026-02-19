# RFC-0006: Character System Decomposition

* **Status:** Implemented
* **Date:** February 2026
* **Prior Art:** `docs/thoughts/0006-refactor-character-system.md`

## Summary

Decompose the monolithic `CharacterSystem` into three single-responsibility systems
(`CharacterInputSystem`, `CharacterStateSystem`, `CharacterMotorSystem`) communicating
through two new intermediate components (`CharacterIntent`, `CharacterState`). Split
the builder-specific fields out of `PlayerState` into `CharacterState` to give each
component a clear owner.

## Motivation

`CharacterSystem::Update` currently does four unrelated things in a single function:

1. **Input translation** — reads `PlayerInput` and converts stick/key values into a
   3D movement vector using the camera's view directions.
2. **State machine** — tracks ground state, jump eligibility, coyote time, and jump
   count.
3. **Physics execution** — calls `CharacterVirtual::SetLinearVelocity` and
   `ExtendedUpdate`.
4. **Transform sync** — copies the Jolt character's world position back to
   `LocalTransform` / `WorldTransform`.

Consequences of this coupling:

- **Untestable state logic.** There is no way to exercise the jump-eligibility or
  coyote-time logic without a live Jolt character and a real `InputRecord`. A unit
  test for "double-jump from airborne state" cannot be written today.
- **AI is impossible.** Any NPC or replay system would need to fake `PlayerInput`
  values that were designed for a human. An intent layer owned by the
  character (not the input hardware) is the standard solution.
- **Physics sync is buried.** Transform synchronisation is easy to miss when
  reading the physics phase — it silently lives inside a Logic-phase system.

## Design

### Overview

Replace `CharacterSystem` with a three-step pipeline that passes data forward via
components:

```
PlayerInputSystem   (PreUpdate)
       │  writes PlayerInput
       ▼
CameraSystem        (Logic — runs first, writes view dirs into PlayerInput)
       │  reads PlayerInput.view_forward / view_right
       ▼
CharacterInputSystem (Logic)
       │  writes CharacterIntent
       ▼
CharacterStateSystem (Logic)
       │  reads CharacterHandle (ground query), CharacterIntent
       │  writes CharacterState
       ▼
PlatformBuilderSystem (Logic)
       │  reads PlayerInput, PlayerState
       ▼
CharacterMotorSystem (Logic — must be last before Physics)
       │  reads CharacterIntent, CharacterState
       │  writes CharacterHandle (velocity commands to Jolt)
       ▼
PhysicsSystem       (Physics — fixed 60 Hz)
       │  steps Jolt, syncs transforms
```

Each system has one clear read/write contract. No system owns more than one concern.

### New Components

```cpp
// src/components.hpp

// Written by CharacterInputSystem, read by CharacterStateSystem + CharacterMotorSystem.
// Represents what the character wants to do this frame — independent of input source.
struct CharacterIntent {
    ecs::Vec3 move_dir        = {0, 0, 0}; // World-space, magnitude <= 1
    ecs::Vec3 look_dir        = {0, 0, 1}; // World-space unit vector
    bool      jump_requested  = false;
    bool      sprint_requested = false;
};

// Written by CharacterStateSystem, read by CharacterMotorSystem.
// Owns the authoritative physics-side state for a character.
struct CharacterState {
    enum class Mode { Grounded, Airborne };

    Mode  mode                  = Mode::Grounded;
    int   jump_count            = 0;
    float air_time              = 0.0f;
    float coyote_time_remaining = 0.0f;
};
```

### Modified Components

`PlayerState` loses the two fields that belong to the character physics layer and
retains only the builder-specific fields it actually owns:

```cpp
// Before
struct PlayerState {
    int   jump_count        = 0;   // ← moves to CharacterState
    float air_time          = 0.0f; // ← moves to CharacterState
    float build_cooldown    = 0.0f;
    bool  trigger_was_down  = false;
};

// After
struct PlayerState {
    float build_cooldown   = 0.0f;
    bool  trigger_was_down = false;
};
```

### System Contracts

**`CharacterInputSystem`** (Logic, runs after `CameraSystem`)
- Reads: `PlayerInput` (move_input, look_input, jump, view_forward, view_right)
- Writes: `CharacterIntent` on the same entity
- Logic: project `move_input` onto the xz-plane using `view_forward`/`view_right`,
  clamp magnitude to 1, copy `jump` → `jump_requested`.
- No Jolt calls, no state reads.

**`CharacterStateSystem`** (Logic, runs after `CharacterInputSystem`)
- Reads: `CharacterIntent`, `CharacterHandle` (ground state query via
  `CharacterVirtual::GetGroundState()`)
- Writes: `CharacterState` (mode, jump_count, air_time, coyote_time_remaining)
- Logic: the complete state machine currently embedded in `CharacterSystem::Update`
  — ground transition, coyote-time countdown, jump eligibility check, jump-count
  increment.
- No velocity application, no Raylib calls.

**`CharacterMotorSystem`** (Logic, runs last before Physics)
- Reads: `CharacterIntent`, `CharacterState`, `CharacterHandle`
- Writes: calls `CharacterVirtual::SetLinearVelocity` / `ExtendedUpdate`
- Logic: acceleration curves (15 m/s² ground, 5 m/s² air), gravity constants
  (40 down, 25 up), jump impulses (12 initial, 10 double).
- No state mutation, no ECS structural changes.

### Lifecycle Hooks

The `Register()` methods use `on_add<CharacterControllerConfig>` to auto-provision
the new components when a character entity is created:

```cpp
// CharacterInputSystem::Register
world.on_add<CharacterControllerConfig>([](ecs::World& w, ecs::Entity e, auto&) {
    w.add(e, CharacterIntent{});
});

// CharacterStateSystem::Register
world.on_add<CharacterControllerConfig>([](ecs::World& w, ecs::Entity e, auto&) {
    w.add(e, CharacterState{});
});
```

`CharacterMotorSystem::Register` retains the existing Jolt `CharacterVirtual`
creation logic from `CharacterSystem::Register` (unchanged).

### Files Changed

| Action | Path |
|--------|------|
| Add | `src/systems/character_input.hpp` |
| Add | `src/systems/character_input.cpp` |
| Add | `src/systems/character_state.hpp` |
| Add | `src/systems/character_state.cpp` |
| Add | `src/systems/character_motor.hpp` |
| Add | `src/systems/character_motor.cpp` |
| Delete | `src/systems/character.hpp` |
| Delete | `src/systems/character.cpp` |
| Modify | `src/components.hpp` — add `CharacterIntent`, `CharacterState`; trim `PlayerState` |
| Modify | `src/main.cpp` — update `Register` calls and pipeline system order |
| Modify | `CMakeLists.txt` — replace `character.cpp` with three new `.cpp` files |

### Migration

`main.cpp` currently calls:
```cpp
CharacterSystem::Register(world);
pipeline.add_logic([](auto& w, float dt){ CharacterSystem::Update(w, dt); });
```

Replace with:
```cpp
CharacterInputSystem::Register(world);
CharacterStateSystem::Register(world);
CharacterMotorSystem::Register(world);

// Logic phase — order matters
pipeline.add_logic([](auto& w, float dt){ CameraSystem::Update(w, dt); });
pipeline.add_logic([](auto& w, float dt){ CharacterInputSystem::Update(w, dt); });
pipeline.add_logic([](auto& w, float dt){ CharacterStateSystem::Update(w, dt); });
pipeline.add_logic([](auto& w, float dt){ PlatformBuilderSystem::Update(w, dt); });
pipeline.add_logic([](auto& w, float dt){ CharacterMotorSystem::Update(w, dt); });
```

`PlatformBuilderSystem` is unaffected — it reads `PlayerInput` and `PlayerState`,
neither of which changes structurally (only `PlayerState` loses two fields it did
not use anyway).

## Alternatives Considered

**Two systems (CharacterInput + CharacterPhysics).** Rejected. The state machine
(coyote time, jump eligibility) is testable independently of both input and physics.
Fusing it with the motor system would leave it as untestable as today.

**Keep `CharacterSystem`, extract pure helper functions.** Rejected. Function
extraction is not enough — the shared mutable `PlayerState` fields would still
couple input and physics paths, and the system would still need a Jolt context to
test jump logic.

**Move transform sync out of PhysicsSystem into a separate step.** Out of scope
for this RFC. `PhysicsSystem` already calls `propagate_transforms()` after
`step()`; the motor system does not need to duplicate this.

## Testing

**Unit tests (no Jolt, no Raylib):**

```cpp
// Inject intent + handle state, verify CharacterState transitions
TEST("jump from grounded increments jump_count") {
    CharacterIntent intent { .jump_requested = true };
    CharacterState  state  { .mode = CharacterState::Mode::Grounded };
    // call CharacterStateSystem::tick(intent, state, ground_state=OnGround, dt)
    REQUIRE(state.jump_count == 1);
    REQUIRE(state.mode == CharacterState::Mode::Airborne);
}

TEST("coyote window expires after 0.2s") {
    CharacterState state { .mode = Mode::Airborne, .coyote_time_remaining = 0.2f };
    CharacterStateSystem::tick(..., dt = 0.21f);
    REQUIRE(state.coyote_time_remaining == 0.0f);
}
```

**Functional verification:**
- All existing movement, jumping, and double-jump behaviour remains identical.
- Platform building still triggers on trigger press with cooldown intact.

## Risks & Open Questions

- **View-direction coupling.** `CharacterInputSystem` reads `PlayerInput::view_forward`
  and `view_right`, which are written by `CameraSystem` — preserving an existing
  coupling. RFC-0007 addresses this by moving those fields to the `MainCamera`
  resource.
- **System count.** Three Logic systems replace one. Per-entity overhead is
  negligible at character counts < 100; the separation benefit clearly outweighs it.
- **Ordering fragility.** `CharacterMotorSystem` must execute before `PhysicsSystem`
  or velocity commands are dropped for a frame. This constraint should be documented
  in `pipeline.hpp` comments once implemented.
