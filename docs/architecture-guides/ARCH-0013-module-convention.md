# ARCH-0013: Module Convention

* **RFC Reference:** [RFC-0013 — Module Convention](../rfcs/02-implemented/0013-module-convention.md)
* **Implementation Date:** 2026-02-21
* **Status:** Active

---

## 1. High-Level Mental Model

> A module is a subsystem's *installer*, not its *logic*. Logic lives in
> `src/systems/`. A module is a thin header in `src/modules/` that wires one
> subsystem into the engine in one place: resources, lifecycle hooks, event
> queues, pipeline steps, and debug rows — all at once, owned by the module
> itself.

The module convention solves a maintenance problem: before RFC-0013, adding a
new system meant touching `main.cpp` in up to four separate places across ~80
lines of setup code. After RFC-0013, `main.cpp` is a sequenced list of
`Module::install(world, pipeline)` calls — a wiring manifest that reads like a
table of contents for the engine's active subsystems.

---

## 2. The Grand Tour

### File locations

```
src/
  modules/                      ← module headers (wiring only, no logic)
    event_bus_module.hpp
    input_module.hpp
    physics_module.hpp
    render_module.hpp
    audio_module.hpp
    debug_module.hpp
    camera_module.hpp
    character_module.hpp
    builder_module.hpp
  systems/                      ← system implementations (unchanged)
    physics.cpp / .hpp
    renderer.cpp / .hpp
    audio.cpp / .hpp
    ...
  main.cpp                      ← wiring manifest (calls Module::install)
```

Modules are **header-only**. They contain no logic and carry no compiled
symbols. Adding a module incurs no change to `CMakeLists.txt`.

### The complete installed system after RFC-0013

| Module | Phase(s) added | Resources created |
|--------|---------------|-------------------|
| `EventBusModule` | Pre-Update (flush) | `EventRegistry` |
| `InputModule` | Pre-Update (gather, player) | — |
| `PhysicsModule` | Physics (step, propagate) | `PhysicsContext` |
| `RenderModule` | Render (3D scene) | `AssetResource`, `MainCamera` |
| `DebugModule` | Render (overlay) | `DebugPanel` (Engine rows) |
| `CameraModule` | Logic[1] (camera) | — (adds Camera debug row) |
| `CharacterModule` | Logic[2,3] (char_input, char_state) | — (adds Character debug rows; registers event queues) |
| `AudioModule` | Logic[4] (audio SFX) | `AudioResource` |
| `BuilderModule` | Logic[5] (platform builder) | — |
| `CharacterModule::install_motor` | Logic[6] (char_motor) | — |

---

## 3. Anatomy of a Module

Every module is a struct with one or two static methods:

```cpp
struct SomeModule {
    // Required. Complete setup: resources, hooks, event queues, pipeline steps.
    static void install(ecs::World& world, ecs::Pipeline& pipeline);

    // Optional. Release resources that need explicit teardown.
    static void shutdown(ecs::World& world);
};
```

### What `install` is responsible for

`install` is the single entry point for a subsystem. Everything that belongs to
that subsystem is wired here. Concretely, it may:

1. **Create and register world resources**
   ```cpp
   AssetResource assets; assets.load(); world.set_resource(assets);
   ```

2. **Call `System::Register(world)`** — installs ECS lifecycle hooks
   (`on_add` / `on_remove`):
   ```cpp
   PhysicsSystem::Register(world);
   ```

3. **Register event queues** on the `EventRegistry` resource:
   ```cpp
   world.resource<EventRegistry>().register_queue<JumpEvent>(world);
   ```

4. **Add pipeline steps** via `pipeline.add_pre_update/logic/physics/render`:
   ```cpp
   pipeline.add_physics([](ecs::World& w, float dt) {
       PhysicsSystem::Update(w, dt);
       ecs::propagate_transforms(w);
   });
   ```

5. **Optionally add debug rows** to the `DebugPanel` resource, guarded by
   `try_resource` so the module works without the debug overlay:
   ```cpp
   if (auto* panel = world.try_resource<DebugPanel>()) {
       panel->watch("Character", "Mode", [&world]() { ... });
   }
   ```

### What `install` is NOT responsible for

- Business logic — that stays in `src/systems/`
- Ordering relative to other modules' *resources* — resource setup is order-
  independent (resources are set before the game loop, consumed during it)
- Per-frame state — `install` runs once; state lives in resources or components

### What `shutdown` is responsible for

Releasing resources that require an explicit API call (GPU memory, audio
handles, OS handles):
```cpp
static void shutdown(ecs::World& world) {
    world.resource<AssetResource>().unload();  // UnloadShader
}
```
`shutdown` is called after the game loop exits, before `CloseWindow()`.

---

## 4. The `main.cpp` Wiring Manifest

After RFC-0013, `main.cpp` is ~60 lines. Its structure is fixed:

```
InitWindow / SetTargetFPS
World + Pipeline creation
Engine module installs    ← order flexible within group
Game module installs      ← order is a hard constraint
SceneLoader::load
Game loop
Shutdown
CloseWindow
```

### Engine vs. Game module groups

The "Engine Modules" group contains modules that add steps only to Pre-Update,
Physics, and Render phases. Their internal install order is flexible because
those phases have no intra-group ordering constraints.

The "Game Modules" group contains modules that add Logic steps. The Logic phase
is order-sensitive: `Pipeline::add_logic` appends in call order, so **the
install call order directly determines the Logic execution order**.

```cpp
// --- Engine Modules ---
EventBusModule::install(world, pipeline);  // Pre-Update: flush
InputModule::install(world, pipeline);     // Pre-Update: gather, player_input
PhysicsModule::install(world, pipeline);   // Physics:    step, propagate
RenderModule::install(world, pipeline);    // Render:     3D scene
DebugModule::install(world, pipeline);     // Render:     overlay

// --- Game Modules (Logic ordering enforced by install order) ---
CameraModule::install(world, pipeline);             // Logic[1]
CharacterModule::install(world, pipeline);          // Logic[2,3]
AudioModule::install(world, pipeline);              // Logic[4]
BuilderModule::install(world, pipeline);            // Logic[5]
CharacterModule::install_motor(world, pipeline);    // Logic[6]
```

Reading this sequence tells you the complete execution order of the engine.
No other file needs to be consulted.

---

## 5. The `install_motor` Pattern — Handling Ordering Splits

`CharacterMotorSystem` must run *after* `AudioSystem` and
`PlatformBuilderSystem` in the Logic phase, but it belongs conceptually to
`CharacterModule`. A single `CharacterModule::install` cannot add both
`CharacterState` and `CharacterMotor` to logic with other systems in between.

The solution is a second entry point that adds only the motor step:

```cpp
struct CharacterModule {
    static void install(ecs::World&, ecs::Pipeline&);       // CharInput, CharState
    static void install_motor(ecs::World&, ecs::Pipeline&); // CharMotor (last Logic)
};
```

This pattern makes the ordering constraint **visible in `main.cpp`** rather
than hidden inside a monolithic install call:

```cpp
CharacterModule::install(world, pipeline);         // sets up state machine
AudioModule::install(world, pipeline);             // audio must follow CharState
BuilderModule::install(world, pipeline);           // builder must follow CharState
CharacterModule::install_motor(world, pipeline);   // motor must follow all of the above
```

The split is intentional and documented. A future pipeline scheduler with
`before`/`after` dependency declarations would eliminate the need for
`install_motor` entirely — but that is an ECS-level change deferred to a
later RFC.

When you see `SomeModule::install_foo`, it always means: *"this step has a
specific position requirement that cannot be satisfied by a single install call
in the current append-only pipeline."*

---

## 6. Debug Row Co-location

A subtle but important pattern: each module registers its own debug rows. The
`DebugModule` creates the `DebugPanel` and registers Engine-level rows (FPS,
Frame Time, Entity count). Every subsequent module that installs after
`DebugModule` can add its own rows:

```cpp
// In CameraModule::install:
if (auto* panel = world.try_resource<DebugPanel>()) {
    panel->watch("Camera", "Mode", [&world]() { ... });
}

// In CharacterModule::install:
if (auto* panel = world.try_resource<DebugPanel>()) {
    panel->watch("Character", "Mode",       [&world]() { ... });
    panel->watch("Character", "Jump Count", [&world]() { ... });
    panel->watch("Character", "Air Time",   [&world]() { ... });
}
```

The `try_resource` guard is critical: it means modules that use this pattern
work correctly whether or not `DebugModule` is installed. A headless test
harness, a stripped release build, or a future game that omits the overlay can
install modules in any combination without changing module code.

This is **ownership of debug data**: the module that owns the system owns its
diagnostic output. No central "debug configuration" file needs to be updated
when a new module is added.

**Installation order dependency:** `DebugModule` must be installed *before* any
module that adds debug rows. In `main.cpp`, `DebugModule` is the last engine
module and the first entity that game modules depend on. This is why it appears
at the bottom of the Engine group.

---

## 7. Data Flow Through Module Installation

Here is the complete sequence of world state changes that `main.cpp` drives,
module by module:

```
EventBusModule::install
  world: EventRegistry{}
  pipeline.pre_update[0]: flush_all()

InputModule::install
  pipeline.pre_update[1]: InputGatherSystem::Update
  pipeline.pre_update[2]: PlayerInputSystem::Update

PhysicsModule::install
  world: shared_ptr<PhysicsContext>
  ECS hooks: on_add<RigidBodyConfig> → create Jolt body
             on_remove<RigidBodyConfig> → destroy Jolt body
  pipeline.physics[0]: PhysicsSystem::Update + propagate_transforms

RenderModule::install
  world: AssetResource (loaded shaders)
  world: MainCamera{}
  pipeline.render[0]: RenderSystem::Update

DebugModule::install
  world: DebugPanel { Engine rows: FPS, Frame Time, Entities }
  pipeline.render[1]: DebugSystem::Update

CameraModule::install
  DebugPanel: + "Camera / Mode" row
  pipeline.logic[0]: CameraSystem::Update

CharacterModule::install
  ECS hooks: on_add<CharacterControllerConfig> → create Jolt character
             on_add<CharacterHandle> → (motor binding)
  EventRegistry: register_queue<JumpEvent>
  EventRegistry: register_queue<LandEvent>
  DebugPanel: + "Character / Mode", "Character / Jump Count", "Character / Air Time"
  pipeline.logic[1]: CharacterInputSystem::Update
  pipeline.logic[2]: CharacterStateSystem::Update

AudioModule::install
  Raylib: InitAudioDevice()
  world: AudioResource (loaded Sound handles)
  pipeline.logic[3]: AudioSystem::Update

BuilderModule::install
  pipeline.logic[4]: PlatformBuilderSystem::Update

CharacterModule::install_motor
  pipeline.logic[5]: CharacterMotorSystem::Update
```

After all installs, `SceneLoader::load` creates entities with components,
triggering the on_add hooks that were registered by PhysicsModule and
CharacterModule.

---

## 8. How to Add a New Module

Adding a new subsystem after RFC-0013 requires four steps:

### Step 1 — Create the system files (unchanged from before)

```
src/systems/my_system.hpp
src/systems/my_system.cpp    ← add to CMakeLists.txt demo target if it has a .cpp
```

### Step 2 — Create the module header

```cpp
// src/modules/my_module.hpp
#pragma once
#include "../pipeline.hpp"
#include "../systems/my_system.hpp"
#include <ecs/ecs.hpp>

struct MyModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        // resource setup
        // System::Register(world) if hooks are needed
        // pipeline.add_XXX(...)

        // optional debug rows
        if (auto* panel = world.try_resource<DebugPanel>())
            panel->watch("My System", "Key Metric", [&world]() { ... });
    }
    // static void shutdown(ecs::World&) { ... }  // if needed
};
```

### Step 3 — Add one line to `main.cpp`

```cpp
#include "modules/my_module.hpp"
// ...
MyModule::install(world, pipeline);   // place in the correct group and position
```

### Step 4 — Write tests for the system logic (not the module)

Module wiring contains no testable logic. Test the system's `Update` function
or the `Register`-installed hooks directly in `tests/logic_tests.cpp`.

That is the complete workflow. `main.cpp` remains a wiring manifest.

---

## 9. System Integration — The Social Map

```
main.cpp
  ├── EventBusModule ─────────────────► EventRegistry (world resource)
  ├── InputModule ─────────────────────► InputRecord (world resource, via InputGatherSystem)
  ├── PhysicsModule ───────────────────► PhysicsContext (world resource)
  │                                      RigidBodyHandle (entity component, via on_add hook)
  ├── RenderModule ────────────────────► AssetResource, MainCamera (world resources)
  ├── DebugModule ─────────────────────► DebugPanel (world resource, Engine rows)
  │
  ├── CameraModule ────────────────────► DebugPanel += Camera rows
  │                                      (reads MainCamera, writes view_forward/view_right)
  ├── CharacterModule ─────────────────► DebugPanel += Character rows
  │                                      EventRegistry += JumpEvent, LandEvent queues
  │                                      CharacterHandle (entity component, via on_add hook)
  ├── AudioModule ─────────────────────► AudioResource (world resource)
  │                                      (reads Events<JumpEvent>, Events<LandEvent>)
  ├── BuilderModule ───────────────────► (deferred entity creation)
  └── CharacterModule::install_motor ──► (writes Jolt velocities, LocalTransform)
```

---

## 10. Trade-offs & Design Decisions

### Modules are not runtime objects

Modules are pure namespaces (structs with only static methods). They have no
instance, no state, no destructor. This is intentional: a module's job is
*one-time wiring*; all runtime state lives in ECS resources or components.

### `shutdown` is explicit, not automatic

Resources stored in the World are destroyed when the World goes out of scope.
But some teardown must precede `CloseWindow()` (GPU resources) or
`CloseAudioDevice()` (audio handles). Explicit `shutdown` calls in `main.cpp`
make this ordering visible. There is no magic RAII wrapper that "does the right
thing" invisibly.

### Module headers include engine headers transitively

A module header includes its system headers, which include Jolt or Raylib. This
means modules are not includable in headless test targets. This is correct:
modules are wiring for the full engine binary. The underlying `debug_panel.hpp`
and `events.hpp` headers remain engine-free and are tested headlessly.

### `install_motor` is not a smell

Splitting CharacterModule into `install` + `install_motor` might look like a
leaky abstraction, but it is honest engineering. The constraint is real:
`CharacterMotorSystem` calls `ExtendedUpdate` on the Jolt character, which
*must* be the last thing that runs before the fixed-step physics tick. A future
pipeline scheduler with dependency declarations would eliminate this pattern —
but it would not eliminate the constraint, only hide it.

### The module convention does not enforce engine/game separation

All modules live in `src/modules/`. Engine modules (`PhysicsModule`,
`RenderModule`) and game modules (`CharacterModule`, `BuilderModule`) share the
directory. The conceptual distinction is documented in `engine-game-separation.md`
and `engine-architecture-strategy.md`. Directory-level enforcement
(`src/engine/` vs `src/game/`) is deferred to Phase 2 (when a second game
begins). The module convention is the prerequisite for that split: once each
subsystem has a single `install` entry point, moving it to a different directory
is a one-line include path change.

---

## 11. Related Resources

* **RFC:** [`docs/rfcs/02-implemented/0013-module-convention.md`](../rfcs/02-implemented/0013-module-convention.md)
* **Module headers:** `src/modules/*.hpp`
* **Wiring manifest:** `src/main.cpp`
* **Architecture analysis:** `docs/thoughts/engine-architecture-strategy.md`
* **ARCH_STATE.md:** Section 4 (System Responsibilities), Section 6 (Module Structure)
