# RFC-0013: Module Convention

* **Status:** Implemented
* **Date:** February 2026
* **Depends on:** All preceding RFCs (0001–0012) — consolidates their setup
  code into a uniform pattern.

## Summary

Introduce a **module convention**: each engine and game subsystem exposes a
`static void install(ecs::World&, ecs::Pipeline&)` function that owns its
complete setup — resource creation, system hook registration, event queue
registration, pipeline wiring, and (optionally) debug panel rows. `main.cpp`
becomes a sequenced list of `Module::install` calls — a wiring manifest that
documents *what* is installed and *in what order*, not *how* each thing is
configured. Subsystems that require graceful shutdown additionally expose a
`static void shutdown(ecs::World&)`.

## Motivation

`main.cpp` currently performs three distinct jobs in one 162-line file:
resource loading, system registration, and pipeline wiring. Adding a new system
requires touching `main.cpp` in up to four separate places (include, resource
setup, Register() call, pipeline lambda). This is an integration friction
problem that grows with every RFC.

Concretely, the current file:

```cpp
// scattered across ~80 lines:
AssetResource assets; assets.load(); world.set_resource(assets);
AudioResource audio;  audio.load();  world.set_resource(audio);
{ DebugPanel panel; panel.watch(/* 6 providers */); world.set_resource(panel); }
PhysicsContext::InitJoltAllocator();
world.set_resource(std::make_shared<PhysicsContext>());
world.set_resource(MainCamera{});
PhysicsSystem::Register(world);
CharacterInputSystem::Register(world);
CharacterStateSystem::Register(world);
CharacterMotorSystem::Register(world);
{ auto& reg = world.resource<EventRegistry>(); reg.register_queue<...>(); }
pipeline.add_pre_update([](ecs::World& w, float) { ... });
// ... 8 more pipeline lambdas ...
```

The module convention collapses this to:

```cpp
EventBusModule::install(world, pipeline);
InputModule::install(world, pipeline);
PhysicsModule::install(world, pipeline);
RenderModule::install(world, pipeline);
AudioModule::install(world, pipeline);
DebugModule::install(world, pipeline);
CameraModule::install(world, pipeline);
CharacterModule::install(world, pipeline);
BuilderModule::install(world, pipeline);
CharacterModule::install_motor(world, pipeline);
```

Adding a future system means: create the module header, call `install` once.
No hunting through `main.cpp` for the right insertion point.

## Design

### The `install` contract

```cpp
struct SomeModule {
    // Required: complete setup — resources, hooks, event queues, pipeline steps.
    static void install(ecs::World& world, ecs::Pipeline& pipeline);

    // Optional: release resources that require explicit teardown.
    static void shutdown(ecs::World& world);
};
```

`install` is the single entry point for a subsystem. It may be called at most
once per world. Its effects are permanent for the lifetime of the world. There
is no `uninstall`.

### The `install_motor` pattern (ordering split)

`Pipeline` appends systems in registration order within each phase. The Logic
phase has a hard ordering constraint: `CharacterMotorSystem` must run *after*
`AudioSystem` and `PlatformBuilderSystem`, but both belong to a single
`CharacterModule` conceptually.

The solution is a second entry point that adds only the motor step:

```cpp
struct CharacterModule {
    static void install(ecs::World&, ecs::Pipeline&);       // CharInput + CharState
    static void install_motor(ecs::World&, ecs::Pipeline&); // CharMotor (last Logic)
};
```

`main.cpp` calls them with other modules in between:

```cpp
CharacterModule::install(world, pipeline);        // CharInput, CharState
AudioModule::install(world, pipeline);            // Audio (after CharState)
BuilderModule::install(world, pipeline);          // Builder
CharacterModule::install_motor(world, pipeline);  // CharMotor (must be last)
```

The split makes the ordering constraint *visible and explicit* in `main.cpp`
rather than hidden inside a single opaque install call.

### Debug row co-location

`DebugModule::install` creates the `DebugPanel` resource and adds Engine-level
rows (FPS, Frame Time, Entity count). Game modules that run after it can then
add their own rows by calling `world.try_resource<DebugPanel>()->watch(...)`.
The `try_resource` guard means modules work correctly even when `DebugModule`
is not installed (e.g., a headless test harness).

### Module locations

All modules live in `src/modules/` as header-only inline structs. They contain
no logic — only wiring. System implementation remains in `src/systems/`.

### Files Changed

| Action | Path | Reason |
|--------|------|--------|
| Create | `src/modules/event_bus_module.hpp` | EventBusModule |
| Create | `src/modules/input_module.hpp` | InputModule |
| Create | `src/modules/physics_module.hpp` | PhysicsModule |
| Create | `src/modules/render_module.hpp` | RenderModule |
| Create | `src/modules/audio_module.hpp` | AudioModule |
| Create | `src/modules/debug_module.hpp` | DebugModule |
| Create | `src/modules/camera_module.hpp` | CameraModule |
| Create | `src/modules/character_module.hpp` | CharacterModule |
| Create | `src/modules/builder_module.hpp` | BuilderModule |
| Modify | `src/main.cpp` | Replace scattered setup with ordered module installs |

No CMakeLists.txt changes — modules are header-only.
No system file changes — logic stays in `src/systems/`.
No test changes — modules contain no testable logic.

## Alternatives Considered

**Single `EngineModule` + `GameModule`.** Coarser grouping reduces the number
of install calls but loses the self-documentation of named modules. Adding a
new system still requires editing an omnibus install function.

**`main.cpp` stays as-is; modules are a future concern.** Defers integration
friction. Valid, but the friction grows every RFC and becomes harder to
untangle later. This RFC is intentionally cheap (no runtime changes, no test
impact) and the right time to do it is before the system count grows further.

**Pipeline scheduler with `before`/`after` constraints.** Would eliminate the
ordering split on `CharacterModule` by letting the scheduler sort systems.
Architecturally correct but requires an ECS pipeline change. Deferred — the
`install_motor` pattern is sufficient for now and makes the constraint explicit.

## Testing

No new testable logic is introduced. Verification is by build + run:
- `cmake --build build --target demo` must succeed.
- `cmake --build build --target unit_tests && ctest` must pass (25 cases).
- Running `./build/demo`: scene loads, character moves, F3 shows debug panel,
  sound plays on jump/land.
