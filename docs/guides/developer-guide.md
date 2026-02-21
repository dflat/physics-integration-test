# Engine Developer Guide

> A complete technical reference for new contributors. After reading this document
> you should be able to navigate the codebase, understand every architectural layer,
> add new systems and modules, extend the ECS, and reason about the performance and
> correctness invariants that underpin the engine.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Repository Layout](#2-repository-layout)
3. [Build System and Dependencies](#3-build-system-and-dependencies)
4. [The ECS Library — Deep Dive](#4-the-ecs-library--deep-dive)
   - 4.1 [Archetypes and Data Layout](#41-archetypes-and-data-layout)
   - 4.2 [Entities](#42-entities)
   - 4.3 [The World API](#43-the-world-api)
   - 4.4 [Resources](#44-resources)
   - 4.5 [Lifecycle Hooks — on_add / on_remove](#45-lifecycle-hooks--on_add--on_remove)
   - 4.6 [The CommandBuffer — Deferred Structural Changes](#46-the-commandbuffer--deferred-structural-changes)
   - 4.7 [Query Caching and Performance](#47-query-caching-and-performance)
   - 4.8 [Math Types](#48-math-types)
   - 4.9 [Transform Propagation](#49-transform-propagation)
5. [The Pipeline](#5-the-pipeline)
   - 5.1 [Phase Execution Model](#51-phase-execution-model)
   - 5.2 [Fixed-Step Physics Integration](#52-fixed-step-physics-integration)
   - 5.3 [Deferred Flush Points](#53-deferred-flush-points)
6. [The Module Convention](#6-the-module-convention)
   - 6.1 [Anatomy of a Module](#61-anatomy-of-a-module)
   - 6.2 [Install Ordering Rules](#62-install-ordering-rules)
   - 6.3 [The install_motor Pattern](#63-the-install_motor-pattern)
7. [Component Design](#7-component-design)
   - 7.1 [The Two-Header Split](#71-the-two-header-split)
   - 7.2 [Component Catalogue](#72-component-catalogue)
   - 7.3 [Tag Components](#73-tag-components)
8. [System Design Patterns](#8-system-design-patterns)
   - 8.1 [The Stateless System Pattern](#81-the-stateless-system-pattern)
   - 8.2 [Register vs Update](#82-register-vs-update)
   - 8.3 [Reading and Writing Resources](#83-reading-and-writing-resources)
9. [Input Pipeline](#9-input-pipeline)
   - 9.1 [InputGatherSystem — Hardware Abstraction](#91-inputgathersystem--hardware-abstraction)
   - 9.2 [PlayerInputSystem — Semantic Translation](#92-playerinputsystem--semantic-translation)
   - 9.3 [CharacterInputSystem — World-Space Projection](#93-characterinputsystem--world-space-projection)
   - 9.4 [Gamepad Heuristics](#94-gamepad-heuristics)
10. [Jolt Physics Integration](#10-jolt-physics-integration)
    - 10.1 [PhysicsContext — The Jolt Runtime](#101-physicscontext--the-jolt-runtime)
    - 10.2 [Layer System](#102-layer-system)
    - 10.3 [RigidBody Lifecycle — on_add Hook](#103-rigidbody-lifecycle--on_add-hook)
    - 10.4 [PhysicsSystem::Update — The Step](#104-physicssystemupdate--the-step)
    - 10.5 [CharacterVirtual — The Player Controller](#105-charactervirtual--the-player-controller)
    - 10.6 [CharacterMotorSystem — Movement and ExtendedUpdate](#106-charactermotorsystem--movement-and-extendedupdate)
    - 10.7 [The MathBridge](#107-the-mathbridge)
    - 10.8 [Transform Synchronisation Flow](#108-transform-synchronisation-flow)
11. [Rendering with Raylib](#11-rendering-with-raylib)
    - 11.1 [Raylib Lifecycle](#111-raylib-lifecycle)
    - 11.2 [AssetResource — Shader Loading](#112-assetresource--shader-loading)
    - 11.3 [RenderSystem — Drawing the Frame](#113-rendersystem--drawing-the-frame)
    - 11.4 [The Lighting Shader](#114-the-lighting-shader)
12. [Camera System](#12-camera-system)
    - 12.1 [Orbit Model](#121-orbit-model)
    - 12.2 [Follow Mode](#122-follow-mode)
    - 12.3 [View Direction Output](#123-view-direction-output)
13. [Character Systems](#13-character-systems)
    - 13.1 [CharacterStateSystem — The State Machine](#131-characterstatesystem--the-state-machine)
    - 13.2 [Coyote Time and Double Jump](#132-coyote-time-and-double-jump)
    - 13.3 [CharacterMotorSystem — Physics Execution](#133-charactermotorsystem--physics-execution)
14. [The Event Bus](#14-the-event-bus)
    - 14.1 [Events\<T\> — The Queue](#141-eventst--the-queue)
    - 14.2 [EventRegistry — Flush Coordination](#142-eventregistry--flush-coordination)
    - 14.3 [Adding a New Event Type](#143-adding-a-new-event-type)
15. [Audio System](#15-audio-system)
16. [The Debug Overlay](#16-the-debug-overlay)
    - 16.1 [DebugPanel — Provider Registry](#161-debugpanel--provider-registry)
    - 16.2 [DebugSystem — Rendering](#162-debugsystem--rendering)
    - 16.3 [Adding Debug Rows](#163-adding-debug-rows)
17. [Scene Serialisation](#17-scene-serialisation)
    - 17.1 [JSON Format](#171-json-format)
    - 17.2 [Spawn Order Invariant](#172-spawn-order-invariant)
    - 17.3 [SceneLoader::unload](#173-sceneloaderunload)
18. [Platform Builder System](#18-platform-builder-system)
19. [Testing](#19-testing)
    - 19.1 [Headless Target vs Demo Target](#191-headless-target-vs-demo-target)
    - 19.2 [Writing Tests](#192-writing-tests)
    - 19.3 [What Not to Test](#193-what-not-to-test)
20. [Adding a New System — Step-by-Step](#20-adding-a-new-system--step-by-step)
21. [Key Invariants and Gotchas](#21-key-invariants-and-gotchas)
22. [Performance Characteristics](#22-performance-characteristics)

---

## 1. Project Overview

This engine is a 3D physics-driven prototype built on three foundational libraries:

| Library | Role |
|---------|------|
| Custom ECS (`extern/ecs`) | Entity/component storage, queries, lifecycle hooks |
| Jolt Physics v5.2.0 | Rigid body simulation, character controller |
| Raylib (master) | Window, input, rendering, audio device |

The design philosophy is **data-oriented separation of concerns**:

- **Components** are pure data — no methods, no inheritance (`src/components.hpp`).
- **Systems** are stateless logic blocks — static methods only (`src/systems/`).
- **Modules** are wiring — header-only, one `install()` per subsystem (`src/modules/`).
- **Resources** are singleton world-state objects accessible by type (`world.set_resource(T)`).
- **main.cpp** is a sequenced list of `Module::install` calls followed by a game loop.

This document covers all of these layers in full depth.

---

## 2. Repository Layout

```
physics-integration-test/
├── extern/
│   └── ecs/                        ← ECS library (git submodule)
│       └── include/ecs/
│           ├── ecs.hpp             ← single-file include
│           ├── world.hpp           ← World class — primary API
│           ├── component.hpp       ← ComponentColumn, type IDs
│           ├── entity.hpp          ← Entity struct
│           ├── command_buffer.hpp  ← deferred structural changes
│           ├── system.hpp          ← SystemRegistry (rarely used directly)
│           └── modules/
│               ├── transform.hpp   ← LocalTransform / WorldTransform
│               ├── hierarchy.hpp   ← Parent / Children components
│               └── transform_propagation.hpp  ← propagate_transforms()
│
├── src/
│   ├── main.cpp                    ← wiring manifest + game loop
│   ├── pipeline.hpp                ← Pipeline: 4-phase frame executor
│   ├── components.hpp              ← game component definitions (engine-free)
│   ├── physics_handles.hpp         ← Jolt runtime handles + MathBridge
│   ├── physics_context.hpp         ← PhysicsContext resource (Jolt init)
│   ├── input_state.hpp             ← InputRecord / GamepadState structs
│   ├── assets.hpp                  ← AssetResource (shaders)
│   ├── audio_resource.hpp          ← AudioResource (Sound handles)
│   ├── events.hpp                  ← Events<T>, EventRegistry, JumpEvent, LandEvent
│   ├── debug_panel.hpp             ← DebugPanel provider registry (engine-free)
│   ├── scene.hpp / scene.cpp       ← SceneLoader — JSON → ECS entities
│   ├── math_util.hpp               ← Camera math helpers
│   ├── modules/                    ← module headers (wiring only)
│   │   ├── event_bus_module.hpp
│   │   ├── input_module.hpp
│   │   ├── physics_module.hpp
│   │   ├── render_module.hpp
│   │   ├── audio_module.hpp
│   │   ├── debug_module.hpp
│   │   ├── camera_module.hpp
│   │   ├── character_module.hpp
│   │   └── builder_module.hpp
│   └── systems/                    ← system logic
│       ├── input_gather.hpp/.cpp
│       ├── player_input.hpp/.cpp
│       ├── camera.hpp/.cpp
│       ├── character_input.hpp/.cpp
│       ├── character_state.hpp/.cpp
│       ├── character_motor.hpp/.cpp
│       ├── physics.hpp/.cpp
│       ├── renderer.hpp/.cpp
│       ├── audio.hpp/.cpp
│       ├── debug.hpp/.cpp
│       └── builder.hpp/.cpp
│
├── tests/
│   ├── main.cpp                    ← Catch2 entry point
│   └── logic_tests.cpp             ← headless unit tests
│
├── resources/
│   ├── scenes/default.json         ← scene definition
│   ├── shaders/lighting.vs/.fs     ← custom lighting shader
│   └── sounds/jump.wav, land.wav   ← audio clips
│
├── docs/
│   ├── rfcs/                       ← RFC pipeline (proposals → active → implemented)
│   ├── architecture-guides/        ← in-depth guides per RFC
│   ├── thoughts/                   ← architectural analysis documents
│   └── guides/                     ← this file
│
├── CMakeLists.txt
└── ARCH_STATE.md                   ← living architecture document
```

---

## 3. Build System and Dependencies

### Prerequisites

- CMake 3.18+, C++20 toolchain (GCC 10+, Clang 11+, or MSVC 2022).
- Git (submodule + FetchContent require network access on first build).

### Build Commands

```bash
# Initialize ECS submodule (first time only)
git submodule update --init

# Debug build
cmake -B build && cmake --build build

# Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# Run the demo
./build/demo

# Run tests
cmake --build build --target unit_tests && cd build && ctest
```

### Dependency Graph

```
demo (executable)
  ├── ecs          — git submodule at extern/ecs, header-only
  ├── Jolt         — FetchContent v5.2.0
  ├── raylib       — FetchContent master
  └── nlohmann_json — FetchContent v3.11.3

unit_tests (executable)
  ├── ecs          — same submodule
  ├── Catch2       — FetchContent v3.4.0
  └── nlohmann_json — same as above
  (NO Jolt, NO raylib — headless by design)
```

### Key CMake Details

**ECS submodule** is brought in via `add_subdirectory`. It exposes a CMake target
called `ecs`.

**Raylib MAX_GAMEPADS patch**: Raylib's default cap is 4 gamepads, which is
insufficient on Linux where every USB device that exposes axes may be reported
as a gamepad. The build system patches `config.h` at download time:

```cmake
PATCH_COMMAND sed -i "s/^[[:space:]]*#[[:space:]]*define[[:space:]]*MAX_GAMEPADS\
  [[:space:]]*[0-9]*/#define MAX_GAMEPADS 16/" src/config.h
```

This is a build-time patch applied once — it does not modify your source tree.

**Two compilation targets**: `demo` links everything (Jolt + Raylib + ECS +
JSON). `unit_tests` links only ECS + Catch2 + JSON. This division is what makes
headless testing possible. Never add Jolt or Raylib includes to files that end up
in `unit_tests`.

**Resources are copied to the build directory** via a `POST_BUILD` cmake command
so that the executable can find shaders and sounds at relative paths.

---

## 4. The ECS Library — Deep Dive

The ECS (`extern/ecs`) is a custom, co-developed, header-only library. It is an
**archetype-based ECS**, which means component data is stored in dense arrays
grouped by the exact set of components each entity has. Understanding its
internals is essential before modifying anything.

### 4.1 Archetypes and Data Layout

An **archetype** is a group of entities that all have the exact same set of
component types. For example, all entities with `{LocalTransform, WorldTransform,
MeshRenderer, RigidBodyConfig, RigidBodyHandle, BoxCollider}` live in one
archetype. All entities with `{LocalTransform, WorldTransform, MeshRenderer,
RigidBodyConfig, RigidBodyHandle, SphereCollider}` live in a different archetype.

Inside an archetype, each component type has its own **column** — a dense
contiguous array of that component's data:

```
Archetype { LocalTransform, WorldTransform, MeshRenderer }

entities[]     = [e1,   e2,   e3  ]   (entity handles, parallel)
LocalTransform = [lt1,  lt2,  lt3 ]   (contiguous, SoA-ish layout)
WorldTransform = [wt1,  wt2,  wt3 ]
MeshRenderer   = [mr1,  mr2,  mr3 ]
```

This layout is **cache-friendly for queries**: when a system iterates all entities
with `LocalTransform` and `WorldTransform`, it linearly traverses two dense arrays
in lockstep. There are no pointer indirections into a sparse per-entity map.

**Column internals** (`ComponentColumn` in `component.hpp`):

```cpp
struct ComponentColumn {
    uint8_t* data;       // raw byte array
    size_t elem_size;    // sizeof(T)
    size_t count;        // number of live elements
    size_t capacity;     // allocated slots
    size_t alignment;

    // Type-erased function pointers — the "vtable"
    MoveFunc    move_fn;       // placement-new move-construct + destroy src
    DestroyFunc destroy_fn;    // ~T() on a slot
    SwapFunc    swap_fn;       // std::swap on two slots
    // ...serialize / deserialize (for trivially-copyable types)
};
```

The `move_fn` and `destroy_fn` are set by `make_column<T>()` at the time the
column is created. They allow the ECS to manage entity lifetimes without knowing
the concrete type at the call site.

**Swap-and-pop removal**: when an entity is removed from an archetype, the last
entity in each column is moved into the vacated slot. This keeps arrays dense but
means **entity order within an archetype is not stable**. Never hold a raw index
into an archetype across a structural change.

**Archetype edges**: the World maintains a graph of archetypes connected by
add/remove edges. When you call `world.add<T>(e, val)`, the World traverses one
edge to find the archetype that has all of `e`'s current components *plus* T, then
migrates `e` there. This lookup is O(1) after the first traversal (cached in the
edge map).

### 4.2 Entities

An `Entity` is two 32-bit integers:

```cpp
struct Entity {
    uint32_t index;       // slot in the entity table
    uint32_t generation;  // version counter for this slot
};
```

When an entity is destroyed, its `index` is put on a free list for reuse. The
`generation` is incremented at destruction time. A stale handle to a destroyed
entity will have a mismatched generation, so `world.alive(e)` will correctly
return `false`. This prevents ABA bugs when entity indices are recycled.

`INVALID_ENTITY` is `{0, 0}`. Index 0 is reserved — the World's constructor
seeds its table with a generation of 1 at slot 0 so that `{0, 0}` can never be
live.

### 4.3 The World API

The `World` is the primary API surface. All data lives inside it — entities,
components, resources, hooks. It is not thread-safe for write operations.

**Creating entities:**

```cpp
// Empty entity — no components
Entity e = world.create();

// Entity with components in one call (preferred — avoids migrations)
Entity e = world.create_with(
    LocalTransform{{0, 1, 0}},
    MeshRenderer{ShapeType::Box, Colors::Red}
);
```

**Adding / removing components:**

```cpp
// Triggers on_add hooks immediately
world.add(e, BoxCollider{{0.5f, 0.5f, 0.5f}});

// Triggers on_remove hooks immediately, then migrates archetype
world.remove<BoxCollider>(e);
```

**Querying component data:**

```cpp
// Direct access — asserts if missing
LocalTransform& lt = world.get<LocalTransform>(e);

// Safe access — returns nullptr if missing or dead
if (auto* lt = world.try_get<LocalTransform>(e)) { ... }

// Check presence
bool has_it = world.has<BoxCollider>(e);
```

**Iterating — the most common operation:**

```cpp
// Each entity with BOTH LocalTransform and WorldTransform
// Callback: (Entity, Ts&...)
world.each<LocalTransform, WorldTransform>(
    [](ecs::Entity e, LocalTransform& lt, WorldTransform& wt) {
        // lt and wt come from the same row — guaranteed to be the same entity's data
        wt.matrix = mat4_compose(lt.position, lt.rotation, lt.scale);
    });
```

**Critical rule**: never call `world.add`, `world.remove`, `world.create`, or
`world.destroy` directly inside an `each` callback. The ECS asserts (`iterating_`
flag) and aborts. Use `world.deferred()` instead (see §4.6).

**Exclude filter — finding entities WITHOUT a component:**

```cpp
// Entities with LocalTransform and WorldTransform, but NOT Parent
world.each<LocalTransform, WorldTransform>(
    World::Exclude<Parent>{},
    [](Entity e, LocalTransform& lt, WorldTransform& wt) { ... });
```

**Single entity — when you know there is exactly one:**

```cpp
// Asserts if 0 or >1 entity matches
world.single<PlayerTag, PlayerInput>(
    [](Entity, PlayerTag&, PlayerInput& input) {
        // process input
    });
```

**Entity count:**

```cpp
size_t total = world.count();             // all living entities
size_t movers = world.count<RigidBodyConfig>(); // entities with this component
```

**Alive check:**

```cpp
if (world.alive(some_entity)) { ... }
```

### 4.4 Resources

Resources are **singleton objects stored in the World by type**. Think of them as
global state scoped to the world's lifetime. Any system can retrieve them by type.

```cpp
// Store a resource (replaces any existing instance of the same type)
world.set_resource(AudioResource{});
world.set_resource(std::make_shared<PhysicsContext>());

// Retrieve — asserts if not present
AudioResource& audio = world.resource<AudioResource>();

// Safe retrieval — returns nullptr if not present
if (auto* audio = world.try_resource<AudioResource>()) {
    audio->snd_jump.Play();
}

// Check existence
bool exists = world.has_resource<MainCamera>();

// Remove
world.remove_resource<DebugPanel>();
```

Resources are stored type-erased as `void*` with a `deleter` function pointer.
When the World is destroyed (goes out of scope), all resources are destroyed in
an unspecified but deterministic order (order of `resources_.clear()`). For
resources requiring teardown *before* `CloseWindow()` (e.g., GPU handles, audio
device), use an explicit `Module::shutdown` call in `main.cpp`.

**PhysicsContext is stored as `shared_ptr<PhysicsContext>`**, not directly. This
is intentional — it allows systems to hold a weak reference without holding the
full Jolt infrastructure by value in a resource slot.

```cpp
// Store:
world.set_resource(std::make_shared<PhysicsContext>());

// Retrieve (note the double dereference):
auto* ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
if (!ctx_ptr || !*ctx_ptr) return;
auto& ctx = **ctx_ptr;
```

### 4.5 Lifecycle Hooks — on_add / on_remove

Lifecycle hooks are callbacks that fire whenever a specific component type is
added to or removed from any entity. They are the ECS's equivalent of Unity's
`Awake`/`OnDestroy` but typed and data-driven.

```cpp
// Register in System::Register(world):
world.on_add<RigidBodyConfig>([&](World& w, Entity e, RigidBodyConfig& cfg) {
    // e now has RigidBodyConfig. Sibling components are already present.
    // Create a Jolt body here and add RigidBodyHandle to e.
    w.add(e, RigidBodyHandle{body->GetID()});
});

world.on_remove<RigidBodyHandle>([&](World& w, Entity, RigidBodyHandle& h) {
    // h is still valid during this callback. Clean up external state.
    bi.RemoveBody(h.id);
    bi.DestroyBody(h.id);
});
```

**Hook execution timing:**

- `on_add` fires *after* the component data is stored in the archetype and
  `records_[e.index]` is updated. This means `world.get<T>(e)` and
  `world.try_get<SiblingComponent>(e)` work correctly inside the hook.
- `on_remove` fires *before* the data is destroyed. The component reference is
  still valid inside the callback.

**Hooks may call `world.add(e, ...)` inside themselves.** This is safe because
the add/remove of the triggering component has already completed before hooks
fire. The new add triggers its own hooks recursively.

**Hooks are registered once and fire for the lifetime of the World.** There is no
way to unregister them. If conditional behavior is needed, check world state
inside the hook.

**Multiple hooks on the same type** are all called in registration order. The
engine uses this for `CharacterControllerConfig` — three separate hooks fire when
that component is added: `CharacterInputSystem::Register` adds `CharacterIntent`,
`CharacterStateSystem::Register` adds `CharacterState`, and
`CharacterMotorSystem::Register` creates the Jolt `CharacterVirtual`.

### 4.6 The CommandBuffer — Deferred Structural Changes

Structural changes (create, destroy, add, remove) are forbidden during `each`
iteration because they would invalidate the archetype column pointers being
iterated. The `CommandBuffer` queues these changes and applies them after
iteration ends.

```cpp
// Queue new entity creation during an each() callback:
world.deferred().create_with(
    LocalTransform{spawn_pos, {0,0,0,1}, size},
    WorldTransform{},
    MeshRenderer{ShapeType::Box, Colors::Maroon},
    BoxCollider{{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f}},
    RigidBodyConfig{BodyType::Static},
    WorldTag{}
);

// Queue component addition:
world.deferred().add(e, SomeComponent{value});

// Queue entity destruction:
world.deferred().destroy(e);

// Queue component removal:
world.deferred().remove<SomeComponent>(e);

// Flush — actually execute all queued commands:
world.deferred().flush(world);
```

The `CommandBuffer` is a flat byte buffer that stores commands in FIFO order.
Components are stored inline (move-constructed into the buffer) — no heap
allocation per command. The buffer itself may grow (it is a `std::vector<uint8_t>`
under the hood), but for typical frame workloads it stays at a stable high-water
mark after the first few frames.

The Pipeline flushes the deferred buffer **at two points per frame** (see §5.3).

### 4.7 Query Caching and Performance

Iterating entities involves identifying which archetypes match the query's
include/exclude component masks. The World caches these matching archetype lists in
a `query_cache_` keyed by `(include_mask, exclude_mask)`. The cache is
invalidated by an `archetype_generation_` counter that increments every time a new
archetype is created.

In steady state (no new archetypes being created), every `each<Ts...>` call is
O(1) to find the matching archetype list, then O(N) to iterate the entities.
The inner loop accesses component data through raw pointers into the dense column
arrays — no virtual dispatch, no pointer indirection:

```cpp
// From world.hpp — the hot loop inside each<Ts...>:
auto ptrs = std::make_tuple(
    static_cast<Ts*>(static_cast<void*>(arch->columns.at(component_id<Ts>()).data))...);
for (size_t i = 0; i < n; ++i)
    fn(arch->entities[i], std::get<Ts*>(ptrs)[i]...);
```

The component arrays are contiguous in memory. Access patterns for typical queries
(2–4 components) are very cache-friendly.

**Archetype fragmentation** is the main performance pitfall: if entities have many
different component combinations, you get many small archetypes. Each archetype
is a separate cache line group. Prefer keeping component sets consistent — use
tags (`PlayerTag`, `WorldTag`) rather than conditionally adding/removing large
components.

### 4.8 Math Types

The ECS provides its own math types in `include/ecs/math.hpp` (pulled in via the
GLM integration). In the engine, we use:

```cpp
ecs::Vec2  // glm::vec2  — 2D float vector
ecs::Vec3  // glm::vec3  — 3D float vector
ecs::Vec4  // glm::vec4  — 4D float vector
ecs::Quat  // glm::quat  — quaternion (x, y, z, w)
ecs::Mat4  // glm::mat4  — column-major 4x4 matrix
```

These types are used in all engine-side structs (components, resources) to avoid
pulling Jolt or Raylib headers into files shared with the headless test target.
Conversion to Jolt or Raylib math types happens at system boundaries via
`MathBridge` and local static helpers (see §10.7 and §11.3).

### 4.9 Transform Propagation

`ecs::propagate_transforms(world)` (in `modules/transform_propagation.hpp`) walks
the entity hierarchy in BFS order and recomputes `WorldTransform` from
`LocalTransform`:

- **Root entities** (have `LocalTransform` + `WorldTransform`, but no `Parent`):
  `WorldTransform.matrix = mat4_compose(position, rotation, scale)`
- **Child entities**: `WorldTransform.matrix = Parent.WorldTransform.matrix * LocalTransform.matrix`

This is called once per physics tick by `PhysicsModule` (see §5.2). After Jolt's
step writes back positions to `LocalTransform`, `propagate_transforms` propagates
those changes up the hierarchy to `WorldTransform`. The render system then reads
`WorldTransform` to place objects in the scene.

```cpp
// Called in PhysicsModule's physics step:
pipeline.add_physics([](ecs::World& w, float dt) {
    PhysicsSystem::Update(w, dt);
    ecs::propagate_transforms(w);   // ← after physics writes LocalTransform
});
```

---

## 5. The Pipeline

The `Pipeline` (`src/pipeline.hpp`) is a simple container of four ordered lists of
system functions. It is the engine's frame executor.

```cpp
class Pipeline {
    using SystemFunc = std::function<void(World&, float)>;

    std::vector<SystemFunc> pre_update_;  // input, event flush
    std::vector<SystemFunc> logic_;       // game logic
    std::vector<SystemFunc> physics_;     // Jolt step
    std::vector<SystemFunc> render_;      // Raylib drawing
};
```

Systems are added to these lists by calling `pipeline.add_pre_update(fn)`,
`pipeline.add_logic(fn)`, `pipeline.add_physics(fn)`, `pipeline.add_render(fn)`.
These append in call order — there is no priority system. Install order equals
execution order.

### 5.1 Phase Execution Model

Each frame in `main.cpp` calls:

```cpp
pipeline.update(world, dt);        // Pre-Update + Logic + deferred flush
pipeline.step_physics(world, fixed_dt);  // Physics (fixed step, may run 0 or N times)
pipeline.render(world);            // Render
```

`pipeline.update` internally executes:
1. All `pre_update_` systems (input gather, event flush)
2. All `logic_` systems (camera, character input/state, audio, builder, motor)
3. `world.deferred().flush(world)` — spawned entities materialise here
4. A second `world.deferred().flush(world)` for any deferred ops from the flush itself

`pipeline.step_physics` executes all `physics_` systems. It is called inside a
fixed-step accumulator loop in `main.cpp`.

`pipeline.render` executes all `render_` systems (3D scene + debug overlay).

### 5.2 Fixed-Step Physics Integration

Physics runs at a **fixed 60 Hz** regardless of render frame rate:

```cpp
// In main.cpp game loop:
float accumulator = 0.0f;
const float fixed_dt = 1.0f / 60.0f;

while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    pipeline.update(world, dt);           // variable dt

    accumulator += dt;
    while (accumulator >= fixed_dt) {
        pipeline.step_physics(world, fixed_dt);  // fixed dt
        accumulator -= fixed_dt;
    }

    pipeline.render(world);
}
```

At 60 FPS, the physics loop runs exactly once per frame. At 30 FPS, it runs twice.
At 120 FPS, it runs every other frame (accumulator < fixed_dt, loop skipped).

**Why fixed step?** Jolt's integration is numerically stable only with a
consistent timestep. Variable-dt physics produces different trajectories on
different machines and leads to inconsistent jump heights.

**Jolt's internal substeps**: `PhysicsSystem::Update(dt, 1, ...)` passes `1` as
the number of collision substeps. This means Jolt runs one integration step per
our fixed tick. For fast-moving objects you might increase this to 2 or 4 to
prevent tunnelling.

### 5.3 Deferred Flush Points

Two deferred flushes happen inside `pipeline.update`:

```
Pre-Update systems run (event flush, input gather)
Logic systems run (camera → charInput → charState → audio → builder → charMotor)
  └── builder may call world.deferred().create_with(...) here
FLUSH 1: spawned platforms become real entities with on_add hooks firing
           → RigidBodyConfig on_add creates their Jolt bodies
FLUSH 2: any deferred ops produced by flush 1 (rare, but safe)
```

After flush 1, the physics step runs and immediately sees the new Jolt bodies.
This is the correct ordering — without the flush, the platforms would have no
physics for one frame.

---

## 6. The Module Convention

Modules are the engine's wiring layer. Each module owns the complete setup of one
subsystem. See `docs/architecture-guides/ARCH-0013-module-convention.md` for the
full reference; this section covers what you need to know to work with them.

### 6.1 Anatomy of a Module

```cpp
// src/modules/my_module.hpp
#pragma once
#include "../pipeline.hpp"
#include "../systems/my_system.hpp"
#include <ecs/ecs.hpp>

struct MyModule {
    // Required: called once, before the game loop.
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        // 1. Create and register resources
        MyResource res; res.load(); world.set_resource(std::move(res));

        // 2. Register ECS lifecycle hooks
        MySystem::Register(world);

        // 3. Register event queues (if the system emits events)
        world.resource<EventRegistry>().register_queue<MyEvent>(world);

        // 4. Add pipeline steps
        pipeline.add_logic([](ecs::World& w, float dt) {
            MySystem::Update(w, dt);
        });

        // 5. Optionally add debug rows (guarded so module works without DebugModule)
        if (auto* panel = world.try_resource<DebugPanel>())
            panel->watch("My System", "Key Metric", [&world]() { return ...; });
    }

    // Optional: explicit teardown (GPU handles, audio device, etc.)
    static void shutdown(ecs::World& world) {
        world.resource<MyResource>().unload();
    }
};
```

Modules are **header-only**. No `.cpp` file, no entry in `CMakeLists.txt`.

### 6.2 Install Ordering Rules

```cpp
// --- Engine Modules --- (Pre-Update, Physics, Render phases)
// Internal order is flexible — no Logic steps here.
EventBusModule::install(world, pipeline);  // must be first (flushes events)
InputModule::install(world, pipeline);
PhysicsModule::install(world, pipeline);
RenderModule::install(world, pipeline);
DebugModule::install(world, pipeline);     // must be before game modules (creates DebugPanel)

// --- Game Modules --- (Logic phase — ORDER IS A HARD CONSTRAINT)
CameraModule::install(world, pipeline);             // Logic[1]
CharacterModule::install(world, pipeline);          // Logic[2,3]
AudioModule::install(world, pipeline);              // Logic[4]
BuilderModule::install(world, pipeline);            // Logic[5]
CharacterModule::install_motor(world, pipeline);    // Logic[6] — must be last
```

**Why does order matter for game modules?** `Pipeline::add_logic` appends in call
order. The resulting execution sequence in the Logic phase is:
`Camera → CharInput → CharState → Audio → Builder → CharMotor`. Each step reads
data written by the step before it.

### 6.3 The install_motor Pattern

`CharacterMotorSystem` must be the **last Logic step** because it calls Jolt's
`ExtendedUpdate` which must complete before the physics tick. But it logically
belongs to `CharacterModule`, which also installs `CharacterInputSystem` and
`CharacterStateSystem` at Logic[2,3].

If `CharacterModule::install` installed all three, the sequence would be:
`CharInput → CharState → CharMotor → Audio → Builder`, which is wrong. Audio
reads events emitted by `CharState` — that's fine. But `CharMotor` must come
*after* `Builder` has potentially moved platforms.

The solution is a second entry point:

```cpp
struct CharacterModule {
    static void install(ecs::World&, ecs::Pipeline&);       // CharInput + CharState
    static void install_motor(ecs::World&, ecs::Pipeline&); // CharMotor only
};
```

`install_motor` is called after `BuilderModule::install`. The split is intentional
and documented in `main.cpp` and the architecture guide.

---

## 7. Component Design

### 7.1 The Two-Header Split

Components are split across two headers based on their dependencies:

| Header | Dependencies | Used in |
|--------|-------------|---------|
| `src/components.hpp` | ECS + stdlib only | `demo` + `unit_tests` |
| `src/physics_handles.hpp` | Jolt + ECS | `demo` only |

This split is what makes headless testing possible. `unit_tests` links `components.hpp`
but not `physics_handles.hpp`. Never add Jolt or Raylib includes to `components.hpp`.

`components.hpp` also includes `<ecs/modules/transform.hpp>` which provides the
ECS-owned `LocalTransform` and `WorldTransform` types.

### 7.2 Component Catalogue

| Component | Header | Purpose |
|-----------|--------|---------|
| `LocalTransform` | ecs/modules/transform.hpp | Position, Rotation, Scale relative to parent |
| `WorldTransform` | ecs/modules/transform.hpp | Absolute world-space matrix |
| `BoxCollider` | components.hpp | Authoring: half-extents for Jolt box shape |
| `SphereCollider` | components.hpp | Authoring: radius for Jolt sphere shape |
| `RigidBodyConfig` | components.hpp | Authoring: body type, mass, friction, restitution |
| `RigidBodyHandle` | physics_handles.hpp | Runtime: `JPH::BodyID` — links entity to Jolt body |
| `CharacterControllerConfig` | components.hpp | Authoring: height, radius, mass, slope limit |
| `CharacterHandle` | physics_handles.hpp | Runtime: `shared_ptr<JPH::CharacterVirtual>` |
| `CharacterIntent` | components.hpp | Per-frame: move direction, jump request (from Input to State/Motor) |
| `CharacterState` | components.hpp | Per-frame: grounded mode, jump count, air time, jump impulse |
| `MeshRenderer` | components.hpp | Visual: shape type, colour, scale offset |
| `PlayerInput` | components.hpp | Per-frame: semantic input (move, look, jump, build) |
| `PlayerState` | components.hpp | Builder state: cooldown, trigger edge detection |
| `MainCamera` | components.hpp | Camera orbit state + view directions (resource, not component) |

**`MainCamera` note**: Although stored as a world resource (`world.try_resource<MainCamera>()`),
its type is defined in `components.hpp` because it contains only ECS math types and
is shared across multiple systems.

### 7.3 Tag Components

Tags are empty structs used as query filters:

```cpp
struct PlayerTag {};  // marks the player entity
struct WorldTag  {};  // marks entities destroyed on scene reset
```

They have zero size — they exist only to narrow queries:

```cpp
// Find exactly the player's PlayerInput
world.single<PlayerTag, PlayerInput>([&](Entity, PlayerTag&, PlayerInput& input) { ... });

// Find all scene entities to destroy on reset
world.each<WorldTag>([&](Entity e, WorldTag&) { to_destroy.push_back(e); });
```

---

## 8. System Design Patterns

### 8.1 The Stateless System Pattern

Systems are structs with only static methods. They have no instance variables.
All runtime state lives in ECS resources or entity components, not in the system
itself.

```cpp
// src/systems/my_system.hpp
struct MySystem {
    static void Register(ecs::World& world);    // install lifecycle hooks
    static void Update(ecs::World& world, float dt);  // per-frame logic
};

// src/systems/my_system.cpp
void MySystem::Register(ecs::World& world) {
    world.on_add<MyConfig>([](ecs::World& w, ecs::Entity e, MyConfig& cfg) {
        w.add(e, MyHandle{/* create external resource */});
    });
}

void MySystem::Update(ecs::World& world, float dt) {
    world.each<MyHandle, LocalTransform>([&](ecs::Entity e, MyHandle& h, LocalTransform& lt) {
        // update logic
    });
}
```

`Register` is called once at startup (from the module's `install`). `Update` is
called every frame (as a lambda in the pipeline). This separation means you can
call `Register` and `Update` independently in tests without a full engine.

### 8.2 Register vs Update

`Register` is only for **installing ECS hooks** — `on_add` and `on_remove`
callbacks. It is NOT for loading assets, creating resources, or running frame
logic. Asset loading belongs in the module's `install`. Frame logic belongs in
`Update`.

```cpp
// CORRECT
void MySystem::Register(ecs::World& world) {
    world.on_add<MyConfig>([](World& w, Entity e, MyConfig&) {
        w.add(e, MyHandle{});  // create runtime handle when authoring config appears
    });
}

// WRONG — do not load assets here
void MySystem::Register(ecs::World& world) {
    auto& assets = world.resource<AssetResource>();  // BAD: resource may not exist yet
    assets.load_something();
}
```

Some systems (e.g., `AudioSystem`, `PlatformBuilderSystem`) have no `Register`
method at all because they have no lifecycle hooks to install.

### 8.3 Reading and Writing Resources

Always use `try_resource` (not `resource`) in `Update` functions. Resources are
optional — a module might not be installed in a test environment:

```cpp
void MySystem::Update(ecs::World& world, float dt) {
    auto* audio = world.try_resource<AudioResource>();
    if (!audio) return;  // graceful degradation if AudioModule not installed
    // ...
}
```

Use `resource<T>()` (asserting) only when the resource is guaranteed to exist
(e.g., inside a hook that is only registered by the module that also creates
the resource).

---

## 9. Input Pipeline

Input flows through three layers per frame:

```
Hardware (Raylib) → InputRecord → PlayerInput → CharacterIntent
     ↑                  ↑               ↑               ↑
InputGatherSystem   PlayerInputSystem  CharacterInputSystem
(Pre-Update[1])     (Pre-Update[2])    (Logic[2])
```

### 9.1 InputGatherSystem — Hardware Abstraction

`InputGatherSystem::Update` (`systems/input_gather.cpp`) calls Raylib's raw input
functions and stores everything in an `InputRecord` resource:

```cpp
struct InputRecord {
    bool    keys_down[512];         // IsKeyDown(i) for every Raylib key code
    bool    keys_pressed[512];      // IsKeyPressed(i) — true only first frame
    Vector2 mouse_pos, mouse_delta;
    float   mouse_wheel;
    bool    mouse_buttons[8];
    bool    mouse_buttons_pressed[8];
    std::vector<GamepadState> gamepads;  // only real gamepads (filtered)
};
```

This layer exists to **decouple all other systems from Raylib's input API**. No
system other than `InputGatherSystem` and `CameraSystem` calls Raylib input
functions directly. Every other system reads from `InputRecord`.

### 9.2 PlayerInputSystem — Semantic Translation

`PlayerInputSystem::Update` (`systems/player_input.cpp`) translates raw hardware
state into semantic game intent stored in `PlayerInput` on the player entity:

```cpp
struct PlayerInput {
    ecs::Vec2 move_input;      // normalised, -1 to 1 on both axes
    ecs::Vec2 look_input;      // right-stick look direction
    bool      jump;            // true only the frame the button was pressed
    bool      plant_platform;  // true only the frame E/LClick/RT was pressed
    float     trigger_val;     // analog trigger depth (0–1)
};
```

This system handles:
- WASD ↔ gamepad left stick mapping
- Gamepad deadzone filtering (0.15f)
- Move vector normalisation (to prevent diagonal speed boost)
- Jump detection (key press, not key hold)
- Gamepad right trigger normalisation from Raylib's [-1, 1] range to [0, 1]

### 9.3 CharacterInputSystem — World-Space Projection

`CharacterInputSystem::Update` (`systems/character_input.cpp`) takes the 2D
`PlayerInput.move_input` and projects it into 3D world-space using the camera's
current view directions:

```cpp
// From CharacterInputSystem::Update:
const ecs::Vec3 view_fwd   = cam->view_forward;   // written by CameraSystem this frame
const ecs::Vec3 view_right = cam->view_right;

JPH::Vec3 fwd   = MathBridge::ToJolt(view_fwd);
JPH::Vec3 right = MathBridge::ToJolt(view_right);
fwd.SetY(0);    // flatten to xz-plane
right.SetY(0);
fwd.Normalize(); right.Normalize();

// W = forward relative to camera, D = right relative to camera
JPH::Vec3 move = fwd * input.move_input.y + right * input.move_input.x;
intent.move_dir = MathBridge::FromJolt(move);
```

This is why `CameraSystem` must run before `CharacterInputSystem` in the Logic
phase — it writes `view_forward` and `view_right` to `MainCamera`, which
`CharacterInputSystem` reads immediately after.

### 9.4 Gamepad Heuristics

Linux exposes many USB devices as gamepads (mice, headsets, keyboard controllers,
sensors). `InputGatherSystem` filters them using `IsRealGamepad`:

```cpp
static bool IsRealGamepad(int i) {
    if (!IsGamepadAvailable(i)) return false;
    if (GetGamepadAxisCount(i) < 4) return false;  // needs at least 4 axes
    // Name-based blacklist: Keyboard, Mouse, SMC, Accelerometer, etc.
    std::string n = GetGamepadName(i);
    for (const char* b : blacklist) {
        if (n.find(b) != std::string::npos) return false;
    }
    return true;
}
```

The CMake build also patches Raylib's `MAX_GAMEPADS` from 4 to 16 so that the
full range of slots is scanned.

---

## 10. Jolt Physics Integration

### 10.1 PhysicsContext — The Jolt Runtime

`PhysicsContext` (`src/physics_context.hpp`) is a class that owns the entire Jolt
runtime. It is stored as a `shared_ptr<PhysicsContext>` world resource.

```cpp
class PhysicsContext {
public:
    JPH::TempAllocatorImpl*   temp_allocator;   // 10 MB scratch buffer
    JPH::JobSystemThreadPool* job_system;        // hardware_concurrency - 1 threads
    JPH::PhysicsSystem*       physics_system;    // the Jolt world

    BPLayerInterfaceImpl broad_phase_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
    ObjectLayerPairFilterImpl         object_layer_pair_filter;

    static void InitJoltAllocator();  // call before construction
    JPH::BodyInterface& GetBodyInterface();
};
```

The constructor calls `JPH::RegisterTypes()`, creates the allocator, job pool, and
`JPH::PhysicsSystem`. The destructor tears them all down. The `10 MB`
`TempAllocatorImpl` is per-step scratch space for Jolt's broadphase and narrowphase.

**`InitJoltAllocator()`** calls `JPH::RegisterDefaultAllocator()` and must be
called before constructing `PhysicsContext`. This is done in `PhysicsModule::install`.

### 10.2 Layer System

Jolt uses object layers to control which bodies collide with which. The engine
uses two layers:

```cpp
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;  // static bodies
    static constexpr JPH::ObjectLayer MOVING     = 1;  // dynamic + character
}
```

Collision rules:
- `NON_MOVING` bodies collide only with `MOVING` bodies (not with each other).
- `MOVING` bodies collide with everything.

The `BPLayerInterfaceImpl`, `ObjectVsBroadPhaseLayerFilterImpl`, and
`ObjectLayerPairFilterImpl` implement these rules via Jolt's interface contracts.
Static bodies that will never move are placed in `NON_MOVING` to skip the
broadphase entirely for static-vs-static pairs.

### 10.3 RigidBody Lifecycle — on_add Hook

The key insight: Jolt bodies are NOT created by `PhysicsSystem::Update`. They are
created the moment `RigidBodyConfig` is added to an entity. This is an
`on_add<RigidBodyConfig>` hook registered in `PhysicsSystem::Register`:

```cpp
world.on_add<RigidBodyConfig>([&](World& w, Entity e, RigidBodyConfig& cfg) {
    if (w.has<RigidBodyHandle>(e)) return;  // idempotent guard

    // Determine shape from sibling components
    JPH::RefConst<JPH::Shape> shape;
    if (auto* box = w.try_get<BoxCollider>(e)) {
        shape = new JPH::BoxShape(MathBridge::ToJolt(box->half_extents));
    } else if (auto* sphere = w.try_get<SphereCollider>(e)) {
        shape = new JPH::SphereShape(sphere->radius);
    } else {
        shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));  // default
    }

    // Read initial position from LocalTransform or WorldTransform
    JPH::Vec3 pos = ...;
    JPH::Quat rot = ...;

    // Determine Jolt motion type from BodyType enum
    JPH::EMotionType motion = (cfg.type == BodyType::Static)
                             ? JPH::EMotionType::Static
                             : JPH::EMotionType::Dynamic;
    JPH::ObjectLayer layer = (cfg.type == BodyType::Static)
                            ? Layers::NON_MOVING : Layers::MOVING;

    JPH::BodyCreationSettings settings(shape, pos, rot, motion, layer);
    settings.mRestitution = cfg.restitution;
    settings.mFriction    = cfg.friction;
    settings.mIsSensor    = cfg.sensor;

    JPH::Body* body = bi.CreateBody(settings);
    bi.AddBody(body->GetID(), JPH::EActivation::Activate);

    w.add(e, RigidBodyHandle{body->GetID()});  // store the Jolt BodyID on the entity
});
```

**The spawn order invariant**: colliders (`BoxCollider`, `SphereCollider`) must be
added to the entity *before* `RigidBodyConfig`. The hook reads `try_get<BoxCollider>(e)`
which must return non-null for the correct shape to be created. `SceneLoader`
guarantees this order (see §17.2).

The `on_remove<RigidBodyHandle>` hook mirrors this:

```cpp
world.on_remove<RigidBodyHandle>([&](World& w, Entity, RigidBodyHandle& h) {
    bi.RemoveBody(h.id);
    bi.DestroyBody(h.id);
});
```

When an entity is destroyed, the World fires `on_remove` for each component before
freeing the memory. The Jolt body is removed from the simulation before the
`RigidBodyHandle` is destroyed.

### 10.4 PhysicsSystem::Update — The Step

`PhysicsSystem::Update` does two things each fixed tick:

1. **Advance the simulation**: `ctx.physics_system->Update(dt, 1, temp, jobs)`
2. **Sync positions back to ECS**: for each dynamic entity, read the new
   position/rotation from Jolt and write it to `WorldTransform` and `LocalTransform`.

```cpp
// After ctx.physics_system->Update:
world.each<RigidBodyHandle, WorldTransform, RigidBodyConfig>(
    [&](Entity e, RigidBodyHandle& h, WorldTransform& wt, RigidBodyConfig& cfg) {
        if (cfg.type == BodyType::Dynamic) {
            JPH::RVec3 pos; JPH::Quat rot;
            bi.GetPositionAndRotation(h.id, pos, rot);

            wt.matrix = mat4_compose(FromJolt(pos), FromJolt(rot), {1,1,1});

            if (auto* lt = world.try_get<LocalTransform>(e)) {
                lt->position = FromJolt(pos);
                lt->rotation = FromJolt(rot);
            }
        }
    });
```

Static bodies are excluded (their `WorldTransform` is already correct from
scene load). `propagate_transforms` then runs after this to update child
transforms.

### 10.5 CharacterVirtual — The Player Controller

Jolt's `CharacterVirtual` is a non-body character controller that uses Jolt's
collision queries but does not create a physics body. It has its own position and
velocity state, handles slope detection, and performs its own integration.

The `CharacterHandle` component stores a `shared_ptr<JPH::CharacterVirtual>`:

```cpp
struct CharacterHandle {
    std::shared_ptr<JPH::CharacterVirtual> character;
};
```

It is created when `CharacterControllerConfig` is added to an entity
(`CharacterMotorSystem::Register`'s `on_add` hook). The shape is a capsule:

```cpp
new JPH::RotatedTranslatedShapeSettings(
    JPH::Vec3(0, 0.5f * cfg.height, 0),   // offset: centre the capsule vertically
    JPH::Quat::sIdentity(),
    new JPH::CapsuleShapeSettings(0.5f * cfg.height, cfg.radius)
)
```

### 10.6 CharacterMotorSystem — Movement and ExtendedUpdate

`CharacterMotorSystem::Update` (`systems/character_motor.cpp`) is the most complex
system. It runs last in the Logic phase and has strict ordering requirements.

For each entity with `CharacterHandle, CharacterIntent, CharacterState, WorldTransform`:

1. **Horizontal velocity**: lerp toward `intent.move_dir * 10.0f` at 15 m/s²
   (grounded) or 5 m/s² (airborne).

2. **Vertical velocity**: if `state.jump_impulse > 0`, apply it. Otherwise apply
   gravity: −40 m/s² (falling) or −25 m/s² (rising), or 0 (grounded).

3. **Rotation**: smoothly SLERP toward the direction of horizontal travel.

4. **ExtendedUpdate**: this is the critical call:
   ```cpp
   ch->ExtendedUpdate(dt, {0, -9.81f, 0}, ext_settings,
                      bp_filter, obj_filter, body_filter, shape_filter,
                      *ctx.temp_allocator);
   ```
   `ExtendedUpdate` integrates the character's position through the collision world,
   resolves contacts, handles step-up/step-down, and updates ground state. It MUST
   run before the physics tick — if called after, the character would already have
   moved through the Jolt step.

5. **Sync back to ECS**: write `lt->position` and `lt->rotation` from
   `ch->GetPosition()` and `ch->GetRotation()`, then recompute `wt.matrix`.

### 10.7 The MathBridge

`physics_handles.hpp` defines `MathBridge` — a set of inline conversion functions
between ECS math types and Jolt types:

```cpp
namespace MathBridge {
    JPH::Vec3 ToJolt(const ecs::Vec3& v) { return {v.x, v.y, v.z}; }
    JPH::Quat ToJolt(const ecs::Quat& q) { return {q.x, q.y, q.z, q.w}; }

    ecs::Vec3 FromJolt(const JPH::Vec3& v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
    ecs::Quat FromJolt(const JPH::Quat& q) { return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()}; }
}
```

Jolt uses `JPH::Vec3` (4 floats internally, 16-byte aligned) and `JPH::Quat`
(4 floats, x/y/z/w). GLM uses `glm::vec3` (3 floats) and `glm::quat` (w/x/y/z,
note the different member order). The `MathBridge` accessors handle all these
differences.

In system code that includes `physics_handles.hpp`, use `MathBridge::ToJolt` to
pass data to Jolt and `MathBridge::FromJolt` to retrieve it.

Camera and render systems use their own inline helpers to avoid depending on
`physics_handles.hpp`:

```cpp
// In camera.cpp:
static inline Vector3 to_v3(const ecs::Vec3& v) { return {v.x, v.y, v.z}; }
static inline ecs::Vec3 from_v3(const Vector3& v) { return {v.x, v.y, v.z}; }
```

### 10.8 Transform Synchronisation Flow

Understanding who writes `LocalTransform` and `WorldTransform` when is critical:

```
Scene Load
  SceneLoader::spawn_entity() sets LocalTransform + WorldTransform{}
                               (WorldTransform.matrix is identity initially)

Each Fixed Physics Tick (pipeline.step_physics):
  PhysicsSystem::Update()
    → for each Dynamic RigidBody:  writes LocalTransform and WorldTransform
    → for each CharacterVirtual:   NOT here — CharacterMotor ran in Logic phase
                                   and already wrote LocalTransform + WorldTransform
  propagate_transforms()
    → walks hierarchy, recomputes WorldTransform from LocalTransform

Logic Phase (pipeline.update):
  CharacterMotorSystem::Update()
    → after ExtendedUpdate, writes lt->position and lt->rotation and wt.matrix
    → NOTE: this runs BEFORE the physics step, so the position is "predicted"
      for this frame, then corrected next tick if physics disagrees
```

Static bodies never have their `LocalTransform` changed after scene load.
Their `WorldTransform` is correct from the initial `propagate_transforms` pass
and never needs to be updated (they don't move).

---

## 11. Rendering with Raylib

### 11.1 Raylib Lifecycle

Raylib is initialised and torn down explicitly:

```cpp
// In main.cpp — before any Raylib calls:
InitWindow(1280, 720, "Physics Integration - Dynamic Parkour");
SetTargetFPS(60);

// AudioDevice is initialised by AudioModule::install
InitAudioDevice();

// Shutdown order matters:
AudioModule::shutdown(world);   // unload sounds + CloseAudioDevice
RenderModule::shutdown(world);  // unload shaders
CloseWindow();                  // last — destroys OpenGL context
```

**`CloseWindow` must be last.** Unloading shaders after `CloseWindow` would
attempt to free GPU resources after the OpenGL context is gone, which is undefined
behavior.

### 11.2 AssetResource — Shader Loading

`AssetResource` (`src/assets.hpp`) owns the lighting shader and its uniform
locations:

```cpp
struct AssetResource {
    Shader lighting_shader;
    int lightDirLoc, lightColorLoc, ambientLoc;
    int playerPosLoc, shadowRadiusLoc, shadowIntensityLoc;

    void load() {
        lighting_shader = LoadShader(
            "resources/shaders/lighting.vs",
            "resources/shaders/lighting.fs");
        lightDirLoc = GetShaderLocation(lighting_shader, "lightDir");
        // ... cache all uniform locations ...

        // Set static defaults (light direction, colour, ambient)
        Vector3 dir = Vector3Normalize({-0.5f, -1.0f, -0.3f});
        SetShaderValue(lighting_shader, lightDirLoc, &dir, SHADER_UNIFORM_VEC3);
    }

    void unload() { UnloadShader(lighting_shader); }
};
```

Uniform locations are cached on load because `GetShaderLocation` is a string
lookup — do not call it every frame.

### 11.3 RenderSystem — Drawing the Frame

`RenderSystem::Update` (`systems/renderer.cpp`) is a pure consumer — it reads
world state and draws it, writing nothing back to the ECS.

```cpp
void RenderSystem::Update(World& world) {
    BeginDrawing();
    ClearBackground({35, 35, 40, 255});

    // 1. Build Raylib Camera3D from MainCamera resource
    Camera3D camera = {};
    if (auto* cam = world.try_resource<MainCamera>()) {
        camera.position   = {cam->lerp_pos.x, ...};
        camera.target     = {cam->lerp_target.x, ...};
        camera.fovy       = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
    }

    // 2. Update dynamic shader uniforms (player position for shadow blob)
    SetShaderValue(assets->lighting_shader, assets->playerPosLoc, &player_pos, ...);

    // 3. Render all MeshRenderers
    BeginMode3D(camera);
    BeginShaderMode(assets->lighting_shader);
    world.each<WorldTransform, MeshRenderer>([&](Entity e, WorldTransform& wt, MeshRenderer& mesh) {
        rlPushMatrix();
        rlMultMatrixf((float*)&wt.matrix);  // apply world transform
        switch (mesh.shape_type) {
            case ShapeType::Box:     DrawCube({0,0,0}, 1,1,1, col); break;
            case ShapeType::Sphere:  DrawSphere({0,0,0}, 0.5f, col); break;
            case ShapeType::Capsule: DrawCapsule({0,0,0}, {0,1.8f,0}, 0.4f, 8,8, col); break;
        }
        rlPopMatrix();
    });
    EndShaderMode();
    EndMode3D();

    EndDrawing();
}
```

The key pattern: `rlPushMatrix() → rlMultMatrixf(WorldTransform) → draw unit
primitive → rlPopMatrix()`. All meshes are unit-size primitives. The
`WorldTransform` matrix encodes position, rotation, and scale, so the unit
primitive is transformed to the correct world-space size and position.

`wt.matrix.m[12/13/14]` are the translation components of the column-major
matrix (indices 12=X, 13=Y, 14=Z). This is used to extract position for gizmos
and shader uniforms.

### 11.4 The Lighting Shader

The custom GLSL shader (`resources/shaders/lighting.vs/.fs`) implements:
- Directional diffuse lighting (static direction: `-0.5, -1.0, -0.3`)
- Ambient colour
- A screen-space "blob shadow" based on player position — an approximation of
  contact shadow rendered as a darkening circle beneath the character.

The player position is sent every frame via `SetShaderValue(playerPosLoc)`. The
shadow radius and intensity are also per-frame uniforms.

---

## 12. Camera System

`CameraSystem` (`systems/camera.cpp`) runs first in the Logic phase. It reads
`InputRecord` and `PlayerInput`, manages the orbit camera state in `MainCamera`,
and outputs `view_forward`/`view_right` for `CharacterInputSystem`.

### 12.1 Orbit Model

The camera position is computed from spherical coordinates:

```
x = distance * sin(theta) * sin(phi)
y = distance * cos(theta)
z = distance * sin(theta) * cos(phi)
```

where `phi` is the horizontal angle and `theta` is the vertical angle (0 = top,
π/2 = horizon). The camera position is then `player_pos + {x, y, z}`.

Manual orbit is driven by:
- **Mouse right-drag**: `orbit_phi -= mouse_delta.x * 0.005f`
- **Gamepad right stick**: `orbit_phi -= look_input.x * 2.5f * dt`
- **Scroll wheel / shoulder buttons**: zoom level (3 discrete distances: 10, 25, 50)

Theta is clamped to `[0.1, π * 0.45]` to prevent gimbal lock and keep the camera
from going underground.

### 12.2 Follow Mode

Follow mode (`cam.follow_mode = true`, toggled by C/Square) activates automatic
camera rotation to stay behind the player. It only activates after 1 second of
no manual input (`cam.last_manual_move_time > 1.0f`):

```cpp
if (cam.follow_mode && cam.last_manual_move_time > 1.0f) {
    // Smooth the character velocity for stable follow
    sv += (vel - sv) * 5.0f * dt;

    if (speed_sq > 0.1f) {
        // align: how aligned is current camera-to-player with movement direction?
        float alignment = calculate_alignment(move_dir, cam_to_player);

        if (alignment > 0.0f) {
            // compute target orbit angle behind movement direction
            float target_phi = calculate_follow_angle(move_dir.x, move_dir.z);
            float diff = normalize_angle(target_phi - cam.orbit_phi);

            // weight by alignment and speed
            cam.orbit_phi += diff * 5.0f * alignment * speed_factor * dt;
        }
    }
}
```

`calculate_alignment` is the 2D dot product between the movement direction and
the camera-to-player direction. When these are aligned (camera behind player),
alignment approaches 1 and the camera stays put. When the player moves away from
camera, alignment is negative and the camera rotates faster to catch up.

### 12.3 View Direction Output

After computing the camera position, `CameraSystem` calculates and stores the
camera's view directions on `MainCamera`:

```cpp
Vector3 fwd = Vector3Normalize(Vector3Subtract(lerp_target, lerp_pos));
Vector3 right = Vector3CrossProduct(fwd, {0, 1, 0});
cam.view_forward = from_v3(fwd);
cam.view_right   = from_v3(right);
```

These are used by `CharacterInputSystem` in the same Logic frame to project
`PlayerInput.move_input` into world space.

---

## 13. Character Systems

The character's behaviour is implemented across three systems in strict order:

```
CharacterInputSystem → CharacterStateSystem → [Audio, Builder] → CharacterMotorSystem
```

### 13.1 CharacterStateSystem — The State Machine

`CharacterStateSystem::Update` calls the pure function `apply_state`:

```cpp
// Exposed as public static for testability:
static void apply_state(bool on_ground, float dt,
                        const CharacterIntent& intent,
                        CharacterState& state);
```

The state machine has two modes: `Grounded` and `Airborne`.

```
Grounded:
  → if on_ground: reset jump_count=0, air_time=0
  → if jump requested: jump_impulse = 12.0f, jump_count++, → Airborne

Airborne:
  → air_time += dt
  → if on_ground: → Grounded (LandEvent emitted)
  → if jump requested && jump_count < 2:
      → double jump: jump_impulse = 10.0f, jump_count++
```

`jump_impulse` is a **one-frame signal**: it is cleared to 0 at the top of every
`apply_state` call, then set if a jump fires. `CharacterMotorSystem` reads it once
the same frame to apply the vertical impulse.

The system reads ground state from the Jolt character directly:
```cpp
bool on_ground = h.character->GetGroundState() ==
                 JPH::CharacterVirtual::EGroundState::OnGround;
```

### 13.2 Coyote Time and Double Jump

Coyote time (grace period after walking off a ledge) is implemented inside
`apply_state`. An entity can have walked off a ledge (`Airborne`, `jump_count==0`,
`air_time < 0.2f`). In this state, a jump request is treated as the first jump
and consumes both jump slots to prevent a second mid-air jump:

```cpp
// In apply_state (inferred from test coverage):
// Coyote jump: airborne, first jump still available, within window
if (state.mode == Airborne && state.jump_count == 0 && state.air_time < 0.2f) {
    state.jump_impulse = 12.0f;
    state.jump_count = 2;  // immediately exhaust both (no double jump from coyote)
}
```

This is tested explicitly in `logic_tests.cpp`:
```cpp
TEST_CASE("apply_state — coyote jump") {
    state.mode = Airborne; state.jump_count = 0; state.air_time = 0.1f;
    apply_state(false, 0.016f, intent, state);
    CHECK_THAT(state.jump_impulse, WithinRel(12.0f));
    CHECK(state.jump_count == 2);
}
```

### 13.3 CharacterMotorSystem — Physics Execution

See §10.6 for the detailed breakdown. The key ordering constraint: this system
must run after `CharacterStateSystem` (reads `jump_impulse`) and after
`PlatformBuilderSystem` (newly spawned platforms need to be in the Jolt world
before `ExtendedUpdate` sweeps against them). This is why `install_motor` is the
last game module install.

---

## 14. The Event Bus

The event bus provides a **frame-scoped publish-subscribe mechanism** for
decoupled communication between systems within a single frame.

### 14.1 Events\<T\> — The Queue

```cpp
template<typename T>
struct Events {
    void send(T event);
    const std::vector<T>& read() const;
    bool empty() const;
    void clear();
private:
    std::vector<T> buffer_;
};
```

`Events<T>` is stored as a World resource. Systems emit into it via `send()` and
consume from it via `read()`. Multiple systems can read the same queue in the same
frame — reads are non-destructive. The queue is cleared once at the start of the
next frame.

### 14.2 EventRegistry — Flush Coordination

`EventRegistry` tracks all registered event queues and provides `flush_all()`:

```cpp
class EventRegistry {
    std::vector<std::function<void()>> flush_fns_;

    template<typename T>
    void register_queue(ecs::World& world) {
        world.set_resource(Events<T>{});
        flush_fns_.push_back([&world]() {
            if (auto* q = world.try_resource<Events<T>>()) q->clear();
        });
    }

    void flush_all() {
        for (auto& fn : flush_fns_) fn();
    }
};
```

`EventBusModule::install` registers `flush_all` as the first Pre-Update step:

```cpp
pipeline.add_pre_update([](ecs::World& w, float) {
    w.resource<EventRegistry>().flush_all();
});
```

This fires before any Logic system runs, so events from the previous frame are
gone before new ones are emitted.

### 14.3 Adding a New Event Type

1. Define the event struct in `src/events.hpp`:
   ```cpp
   struct MyEvent {
       ecs::Entity entity;
       int some_value;
   };
   ```

2. Register the queue in your module's `install`:
   ```cpp
   world.resource<EventRegistry>().register_queue<MyEvent>(world);
   ```

3. Emit in your emitting system:
   ```cpp
   if (auto* ev = world.try_resource<Events<MyEvent>>())
       ev->send({e, 42});
   ```

4. Consume in your consuming system:
   ```cpp
   if (const auto* evts = world.try_resource<Events<MyEvent>>()) {
       for (const auto& ev : evts->read()) {
           // handle ev
       }
   }
   ```

The consuming system must run in the same frame as the emitting system, after it
in the Logic phase.

---

## 15. Audio System

`AudioSystem` (`systems/audio.cpp`) is a pure consumer of jump and land events.
It has no `Register` method (no lifecycle hooks) and no ECS state of its own.

```cpp
void AudioSystem::Update(World& world, float) {
    auto* audio = world.try_resource<AudioResource>();
    if (!audio) return;

    if (const auto* evts = world.try_resource<Events<JumpEvent>>()) {
        for (const auto& ev : evts->read()) {
            Sound& s = (ev.jump_number == 1) ? audio->snd_jump : audio->snd_jump2;
            PlaySound(s);
        }
    }
    if (const auto* evts = world.try_resource<Events<LandEvent>>()) {
        if (!evts->empty()) PlaySound(audio->snd_land);
    }
}
```

`AudioResource` (`src/audio_resource.hpp`) owns three `Sound` handles (Raylib's
handle type for short audio clips). Loading a file that doesn't exist returns a
zero-initialised `Sound`; `PlaySound` on a zero Sound is a no-op — graceful
degradation.

`AudioModule::install` calls `InitAudioDevice()`, loads sounds, and adds
`AudioSystem` to the Logic phase. `AudioModule::shutdown` unloads sounds and calls
`CloseAudioDevice()`.

---

## 16. The Debug Overlay

The debug overlay is a F3-toggled overlay panel that shows live metrics from any
subsystem. It is designed to be **zero-dependency**: the core (`debug_panel.hpp`)
uses only stdlib and is includable in headless test targets.

### 16.1 DebugPanel — Provider Registry

```cpp
struct DebugPanel {
    using Provider = std::function<std::string()>;

    struct Row     { std::string label; Provider fn; };
    struct Section { std::string title; std::vector<Row> rows; };

    bool visible = false;

    // Register a provider (section created automatically if new)
    void watch(const std::string& section, const std::string& label, Provider fn);

    const std::vector<Section>& sections() const;
private:
    std::vector<Section> sections_;
};
```

Providers are **pull-based** — they are `std::function<std::string()>` callbacks
stored in the panel. Every render frame, `DebugSystem` calls each provider to get
the current value to display. Providers capture references to world state via
lambda closures.

```cpp
// Example provider registered in DebugModule::install:
panel->watch("Engine", "FPS", []() {
    return std::to_string(GetFPS());
});
panel->watch("Engine", "Entities", [&world]() {
    return std::to_string(world.count());
});
```

`DebugModule` creates the `DebugPanel` resource and registers Engine-level rows.
Each subsequent game module that calls `install` after `DebugModule` can add its
own rows:

```cpp
// In CharacterModule::install:
if (auto* panel = world.try_resource<DebugPanel>()) {
    panel->watch("Character", "Mode", [&world]() {
        std::string r = "-";
        world.each<CharacterState>([&](ecs::Entity, CharacterState& s) {
            r = (s.mode == CharacterState::Mode::Grounded) ? "Grounded" : "Airborne";
        });
        return r;
    });
}
```

The `try_resource<DebugPanel>()` guard is essential — it makes modules work
correctly in headless test targets or stripped builds where `DebugModule` is not
installed.

### 16.2 DebugSystem — Rendering

`DebugSystem::Update` (`systems/debug.cpp`) handles the F3 toggle and draws the
panel using Raylib's 2D drawing API. Providers are called lazily — only when the
panel is visible.

```cpp
void DebugSystem::Update(World& world, float) {
    auto* panel = world.try_resource<DebugPanel>();
    if (!panel) return;
    if (IsKeyPressed(KEY_F3)) panel->visible = !panel->visible;
    if (!panel->visible) return;

    // draw background, iterate sections, call row.fn() for each value
    for (const auto& sec : panel->sections()) {
        DrawText(sec.title.c_str(), ...);
        for (const auto& row : sec.rows) {
            std::string val = row.fn();  // pull the current value
            DrawText(row.label.c_str(), ...);
            DrawText(val.c_str(), ...);
        }
    }
}
```

### 16.3 Adding Debug Rows

Add rows in your module's `install`, after `DebugModule::install`:

```cpp
if (auto* panel = world.try_resource<DebugPanel>()) {
    panel->watch("My System", "Some Metric", [&world]() {
        // This lambda runs every render frame (when panel is visible)
        // Must be cheap — no heap allocation, no I/O
        int n = 0;
        world.each<MyComponent>([&](ecs::Entity, MyComponent& c) { n += c.count; });
        return std::to_string(n);
    });
}
```

Be careful with provider performance — they run every render frame when the panel
is open. Avoid allocations. The `std::to_string` and `snprintf` patterns in the
existing code are fine for the frame rate this engine targets.

---

## 17. Scene Serialisation

Scenes are defined in JSON files under `resources/scenes/`. The scene is loaded
once at startup and can be hot-reloaded by pressing R during play.

### 17.1 JSON Format

```json
{
  "entities": [
    {
      "transform": {
        "position": [0.0, 0.0, 0.0],
        "rotation": [0, 0, 0, 1],
        "scale":    [1, 1, 1]
      },
      "mesh": {
        "shape":        "Box",
        "color":        [0.784, 0.784, 0.784, 1.0],
        "scale_offset": [1, 1, 1]
      },
      "box_collider": { "half_extents": [20, 0.5, 20] },
      "rigid_body":   { "type": "Static", "friction": 0.7 },
      "tags": ["World"]
    },
    {
      "transform": { "position": [0, 3, 0], "rotation": [0,0,0,1], "scale": [1,1,1] },
      "mesh":      { "shape": "Capsule", "color": [1, 0, 0, 1] },
      "character": { "height": 1.8, "radius": 0.4, "mass": 70.0, "max_slope_angle": 45 },
      "tags": ["Player", "World"]
    }
  ]
}
```

Supported fields per entity: `transform`, `mesh`, `box_collider`, `sphere_collider`,
`rigid_body`, `character`, `tags`.

Valid `rigid_body.type`: `"Static"`, `"Dynamic"`, `"Kinematic"`.
Valid `mesh.shape`: `"Box"`, `"Sphere"`, `"Capsule"`.
Valid `tags`: `"World"` (destroyed on reset), `"Player"` (also adds `PlayerInput` and `PlayerState`).

### 17.2 Spawn Order Invariant

`SceneLoader::spawn_entity` adds components in a specific order that satisfies the
`on_add` hook preconditions:

```
1. LocalTransform + WorldTransform  — always first (physics reads initial position)
2. BoxCollider / SphereCollider      — before RigidBodyConfig (hook reads collider shape)
3. MeshRenderer                      — visual, order-independent
4. RigidBodyConfig                   — triggers on_add hook → creates Jolt body
5. CharacterControllerConfig         — triggers multiple on_add hooks
6. Tags (WorldTag, PlayerTag, ...)   — last, no hook dependencies
```

This order is a **hard invariant**. If you add `RigidBodyConfig` before
`BoxCollider`, the hook fires without a collider present and falls back to a
default 1m cube shape — probably not what you want. Always add collider configs
before physics configs when constructing entities programmatically.

### 17.3 SceneLoader::unload

```cpp
void SceneLoader::unload(ecs::World& world) {
    std::vector<ecs::Entity> to_destroy;
    world.each<WorldTag>([&](ecs::Entity e, WorldTag&) {
        to_destroy.push_back(e);
    });
    for (auto e : to_destroy) world.destroy(e);
    world.deferred().flush(world);
}
```

This gathers all `WorldTag` entities, destroys them (firing `on_remove` hooks
which clean up Jolt bodies), then flushes deferred commands. After this call, the
scene is clean and `SceneLoader::load` can repopulate it.

The two-step collect-then-destroy pattern is required because `world.destroy`
cannot be called inside `world.each` (structural change during iteration).

---

## 18. Platform Builder System

`PlatformBuilderSystem::Update` (`systems/builder.cpp`) demonstrates the deferred
command pattern for entity creation during iteration:

```cpp
void PlatformBuilderSystem::Update(World& world) {
    float dt = GetFrameTime();
    world.each<PlayerTag, WorldTransform, PlayerInput, PlayerState>(
        [&](Entity, PlayerTag&, WorldTransform& wt, PlayerInput& input, PlayerState& state) {
            // Debounce: rising edge detection on trigger
            bool trigger_pressed = input.plant_platform && !state.trigger_was_down;
            state.trigger_was_down = input.plant_platform;

            if (trigger_pressed && state.build_cooldown <= 0) {
                state.build_cooldown = 0.25f;

                ecs::Vec3 spawn_pos = {player_pos.x, player_pos.y - 0.2f, player_pos.z};
                ecs::Vec3 size      = {4.0f, 0.5f, 4.0f};

                // Queue entity creation — will execute after this each() ends
                world.deferred().create_with(
                    ecs::LocalTransform{spawn_pos, {0,0,0,1}, size},
                    ecs::WorldTransform{},
                    MeshRenderer{ShapeType::Box, Colors::Maroon},
                    BoxCollider{{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f}},
                    RigidBodyConfig{BodyType::Static},
                    WorldTag{}
                );
            }
        });
}
```

The deferred flush in `Pipeline::update` runs after all Logic systems complete,
so the platform entity is fully created (with its Jolt body) before
`CharacterMotorSystem::Update` runs. This is why the flush point between Logic
and Physics exists.

---

## 19. Testing

### 19.1 Headless Target vs Demo Target

The `unit_tests` target is deliberately **headless** — it links only ECS, Catch2,
and nlohmann/json. No Jolt, no Raylib. This means:

- `components.hpp` ✓ (no engine dependencies)
- `debug_panel.hpp` ✓ (stdlib only)
- `events.hpp` ✓ (stdlib only)
- `scene.hpp` / `scene.cpp` ✓ (uses JSON, no Jolt/Raylib)
- `physics_context.hpp` ✗ (Jolt headers)
- `assets.hpp` ✗ (Raylib headers)
- `systems/physics.cpp` ✗ (Jolt)
- `systems/renderer.cpp` ✗ (Raylib)

The `character_state.hpp` exposes `apply_state` as a `static` method precisely
so the core state machine logic can be tested without a Jolt character:

```cpp
// From character_state.hpp:
struct CharacterStateSystem {
    static void Register(ecs::World& world);
    static void Update(ecs::World& world, float dt);
    // Pure function — no ECS, no Jolt. Testable headlessly.
    static void apply_state(bool on_ground, float dt,
                            const CharacterIntent& intent, CharacterState& state);
};
```

### 19.2 Writing Tests

```cpp
#include <catch2/catch_test_macros.hpp>
#include "../src/components.hpp"
#include "../src/events.hpp"
#include <ecs/ecs.hpp>

TEST_CASE("My feature behaves correctly", "[my_tag]") {
    SECTION("Normal case") {
        // Arrange
        CharacterState state{};
        state.mode = CharacterState::Mode::Grounded;

        // Act
        CharacterStateSystem::apply_state(true, 0.016f, {}, state);

        // Assert
        CHECK(state.jump_count == 0);
    }
}
```

Use Catch2's `CHECK` (non-fatal) and `REQUIRE` (fatal on failure). Use
`Catch::Matchers::WithinRel(expected)` for floating-point comparisons.

The `SceneLoader::load_from_string` pattern is powerful for integration-style
tests without file I/O:

```cpp
static const char* SCENE = R"({ "entities": [...] })";

TEST_CASE("Scene loads correctly", "[scene]") {
    ecs::World world;
    REQUIRE(SceneLoader::load_from_string(world, SCENE));
    CHECK(world.count() == 2);
    // query specific entities...
}
```

### 19.3 What Not to Test

- **Module wiring**: `Module::install` contains no logic. It connects components
  you have already tested. Don't test that `install` calls `pipeline.add_logic`
  correctly — just trust it and test at the system level.
- **Physics integration**: Jolt's physics are Jolt's problem. Test the ECS
  structures (hooks, state machines) not Jolt's gravity.
- **Rendering**: Raylib output is visual and untestable without a display.

---

## 20. Adding a New System — Step-by-Step

Here is the complete checklist for adding a new system:

### Step 1: Define any new components

Add to `src/components.hpp` (no Jolt/Raylib):
```cpp
struct MyConfig {
    float speed = 5.0f;
    int   count = 0;
};
```

If you need a Jolt runtime handle, add it to `src/physics_handles.hpp`.

### Step 2: Create the system files

```
src/systems/my_system.hpp
src/systems/my_system.cpp
```

```cpp
// my_system.hpp
#pragma once
#include <ecs/ecs.hpp>

struct MySystem {
    static void Register(ecs::World& world);  // if you need lifecycle hooks
    static void Update(ecs::World& world, float dt);
};

// my_system.cpp
#include "my_system.hpp"
#include "../components.hpp"
// include other headers as needed

void MySystem::Register(ecs::World& world) {
    world.on_add<MyConfig>([](ecs::World& w, ecs::Entity e, MyConfig& cfg) {
        // create runtime state
    });
}

void MySystem::Update(ecs::World& world, float dt) {
    world.each<MyConfig, LocalTransform>([&](ecs::Entity e, MyConfig& cfg, ecs::LocalTransform& lt) {
        // per-frame logic
    });
}
```

### Step 3: Add to CMakeLists.txt

The system `.cpp` must be added to the `demo` target:

```cmake
add_executable(demo
    ...
    src/systems/my_system.cpp  # add here
)
```

If the system has no Jolt/Raylib dependencies and you want to test it headlessly,
also add it to `unit_tests`:
```cmake
add_executable(unit_tests
    ...
    src/systems/my_system.cpp  # add here if headless-safe
)
```

### Step 4: Create the module header

```cpp
// src/modules/my_module.hpp
#pragma once
#include "../pipeline.hpp"
#include "../systems/my_system.hpp"
#include <ecs/ecs.hpp>

struct MyModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        MySystem::Register(world);
        pipeline.add_logic([](ecs::World& w, float dt) { MySystem::Update(w, dt); });

        if (auto* panel = world.try_resource<DebugPanel>())
            panel->watch("My System", "Count", [&world]() { return "..."; });
    }
};
```

### Step 5: Install in main.cpp

```cpp
#include "modules/my_module.hpp"

// In main():
// Engine modules (if it touches only Pre-Update/Physics/Render)
MyModule::install(world, pipeline);

// OR Game modules (if it adds Logic steps), in correct Logic order:
// ... before or after other game modules depending on data dependencies
MyModule::install(world, pipeline);
```

### Step 6: Write tests

Add to `tests/logic_tests.cpp`:
```cpp
TEST_CASE("MySystem correctness", "[my_system]") {
    // Test the pure logic, not the ECS wiring
}
```

---

## 21. Key Invariants and Gotchas

These are the rules you must not break:

**ECS invariants:**

1. **No structural changes during `each`**. Never call `world.add`, `world.remove`,
   `world.create`, or `world.destroy` inside an `each` callback. Use
   `world.deferred()`.

2. **`on_add` hooks see sibling components**. When an `on_add<T>` hook fires, all
   other components added to the entity before T are already present. This is why
   the scene loader adds colliders before physics configs.

3. **Never hold an archetype row index across a structural change**. Archetypes use
   swap-and-pop removal — any add/remove/destroy can move the last entity to any
   vacated slot.

4. **`world.resource<T>()` asserts; `world.try_resource<T>()` returns nullptr**.
   Use `try_resource` in systems and Update functions. Use `resource` only inside
   code that is guaranteed to run after the resource is created (e.g., inside a
   hook registered by the same module that creates the resource).

**Pipeline invariants:**

5. **Logic phase order is a hard constraint**. Camera must precede CharInput; CharState
   must precede CharMotor. Adding a Logic step in the wrong order breaks data
   dependencies.

6. **`ExtendedUpdate` must run before the physics tick**. CharacterMotorSystem is
   always the last Logic step for this reason.

7. **Deferred flush happens after Logic, before Physics**. Entities created by
   `world.deferred().create_with(...)` in the Logic phase appear in Jolt before
   the physics step.

**Jolt invariants:**

8. **`PhysicsContext::InitJoltAllocator()` before constructing `PhysicsContext`**.
   This calls `JPH::RegisterDefaultAllocator()` which must be the first Jolt call.

9. **Collider components must exist before `RigidBodyConfig`**. The `on_add<RigidBodyConfig>`
   hook reads `try_get<BoxCollider>(e)`. If the collider isn't there, you get a
   default 1m cube shape.

10. **`CloseWindow` last**. Calling `UnloadShader` or `UnloadSound` after
    `CloseWindow` is undefined behavior. The shutdown order in `main.cpp` is:
    `AudioModule::shutdown → RenderModule::shutdown → CloseWindow`.

**Component invariants:**

11. **No methods, no inheritance, no virtual functions in components**. Components
    are data. Logic belongs in systems. This is enforced by convention, not by the
    compiler.

12. **No Jolt or Raylib includes in `components.hpp`**. This file is included in
    the headless test target. Jolt/Raylib types belong in `physics_handles.hpp` and
    `assets.hpp`.

---

## 22. Performance Characteristics

Understanding the performance model helps you write efficient systems.

### ECS Query Cost

- **`world.each<A, B, C>`**: O(1) cache lookup + O(N) linear scan per matching
  archetype. In steady state (no new archetypes), the first call for a given
  signature costs one `unordered_map` lookup and a bitset comparison per archetype.
  Subsequent calls hit the query cache.
- **Component access within a query**: raw pointer arithmetic into dense arrays.
  No virtual dispatch, no bounds checks in release. Highly cache-friendly when
  accessing components in iteration order.
- **Archetype fragmentation**: the main performance risk. Many different component
  combinations → many small archetypes → more cache misses per query. Keep
  component sets consistent. Use tags rather than conditionally adding/removing
  large components.

### Adding/Removing Components

Every `world.add<T>(e, ...)` or `world.remove<T>(e)` that changes the entity's
archetype involves:
1. One archetype edge lookup (cached after first traversal, O(1)).
2. Moving all existing component data from old archetype to new.
3. Firing hooks.

This is O(K) where K is the number of components on the entity. For entity
creation at runtime (e.g., spawning platforms), this is acceptable. Avoid adding/
removing components every frame on hundreds of entities.

### Physics

Jolt runs on `hardware_concurrency - 1` threads via `JobSystemThreadPool`. The
10 MB `TempAllocatorImpl` is a stack-based scratch space — no heap allocation
during the physics step itself.

The character's `ExtendedUpdate` involves a series of sweep tests against the
broadphase. For one character on typical geometry, this is negligible.

### Rendering

The render loop is O(N) in the number of entities with `WorldTransform + MeshRenderer`.
Each entity calls `rlPushMatrix`, `rlMultMatrixf`, one draw call, `rlPopMatrix`.
For the current scene (< 100 entities), this is CPU-bound trivially fast.

The shader's shadow blob calculation is per-fragment. It is a simple distance
check against the player position — not a shadow map. This is intentionally cheap.

### Debug Overlay

Debug providers run every render frame when the panel is visible. They involve
`world.each<CharacterState>` queries. These are O(N) over a handful of entities.
Keep providers cheap: use `snprintf`-style formatting, avoid heap allocation.

### Input

`InputGatherSystem` iterates all 512 key codes and all 16 gamepad slots every
frame. This is fast (512 `bool` reads from Raylib's input table). The gamepad
`IsRealGamepad` filter does string comparisons — these are done only on detected
slots, and the blacklist is short.

---

*End of Developer Guide. For the architectural rationale behind any specific
decision, consult the corresponding RFC in `docs/rfcs/02-implemented/` and the
architecture guide in `docs/architecture-guides/`.*
