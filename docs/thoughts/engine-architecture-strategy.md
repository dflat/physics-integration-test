# Engine Architecture Strategy — Performance, Reuse, and Extension

*February 2026. A concrete analysis and actionable recommendations for this project.*

---

## 1. Does the `src/engine/` Split Have Runtime Overhead?

**No. Zero.**

A directory reorganisation and a CMake static-library boundary are purely
build-time constructs. At runtime the resulting binary is identical to the
current flat layout:

- **Static library linkage** (`add_library(engine STATIC ...)`) produces an
  object archive. The linker strips unused symbols and links the rest directly
  into the executable. Same machine code, same address space, same call
  instructions. There is no dispatch table, no symbol lookup, no cache
  boundary.
- **Template instantiation** happens at the call site regardless of directory
  structure. Our ECS queries (`world.each<T>`) are already fully inlined by the
  time the linker runs.
- **No virtual dispatch is introduced** by a directory split alone. Overhead
  from abstract interfaces is a design choice, not a consequence of directory
  organisation.

The *only* way a split introduces overhead is if you simultaneously introduce
a **dynamic library boundary** (`.so` / `.dll`) or **virtual interfaces** as
the API contract. We don't need either. A static library with concrete types
and inlined templates is free.

**Conclusion:** The performance question is a non-question. The split is worth
evaluating entirely on maintenance and reuse grounds.

---

## 2. Fork Per Game vs. Shared Engine

This is the real question, and both are legitimate choices with different
cost structures.

### The fork model

Each new game starts as a copy (or git fork) of the engine. Engine and game
code mix freely. There is no API contract, no submodule version to pin, no
"is this engine or game?" classification.

| Advantage | Disadvantage |
|-----------|-------------|
| Zero abstraction overhead | No propagation of bug fixes or improvements |
| Total freedom to evolve engine for this game's needs | Rebuilding solved problems in each fork |
| No API compatibility concerns | Forked codebases diverge; back-porting is painful |
| Simplest mental model | Can't test "the engine" in isolation |

The fork model is how most indie studios operate implicitly — even if they
don't call it forking. Unity projects are forks; each project accumulates its
own `Assets/Scripts/Engine/` helpers. It works until you want the fourth game
to benefit from something you fixed in the second.

**When the fork model is the right call:** single game, short lifetime,
unlikely to be reused, or a studio that ships one title per decade. For a
project explicitly designed as a reusable engine, it is the wrong model by
definition.

### The shared submodule model

The engine is a separate git repository. Games `git submodule add` it.
Improvements pushed to the engine repository flow into games via `git submodule
update`. This is exactly what this project already does with the ECS:

```
extern/ecs  ←  git submodule (separate repo, pushed independently)
```

The natural extension is to do the same thing one level up:

```
engine/  ←  git submodule  (physics, rendering, audio, input, ECS adapter)
src/     ←  game code      (character, builder, camera, game-specific scenes)
```

| Advantage | Disadvantage |
|-----------|-------------|
| Bug fixes propagate to all games | Breaking engine changes must be versioned |
| "Engine" is tested across multiple game surfaces | More coordination when changing stable API |
| Clear ownership — engine team vs. game team | Submodule friction (pinned commits, update workflow) |
| New game starts with a mature, tested base | Some abstraction tax at the game/engine seam |

**The submodule pattern is already proven at this project's own ECS level.**
The ECS is not duplicated per game; it is shared, pushed, and updated. The
question is whether we apply the same pattern to the *engine* layer. The
answer should be yes — but not yet (see Section 4).

---

## 3. The Real Question: What Is the Reuse Unit?

Before choosing a strategy, you need to be clear about *what* you are reusing.

| Reuse unit | Right boundary | Right mechanism |
|------------|---------------|-----------------|
| Everything (whole engine) | `engine/` submodule | Same as ECS today |
| Physics + rendering only | Separate physics/render repos | Fine-grained submodules |
| Only the ECS substrate | Already done | Keep as-is |
| Nothing (game-specific) | Fork | Don't call it an engine |

For this project the intended reuse unit is: **physics simulation, rendering
pipeline, input aggregation, audio system, event bus, scene loader, debug
overlay**. The character motor, builder, and camera are game code and should
not be part of the engine.

The boundary defined in `engine-game-separation.md` (Section 8) is the correct
starting point. The five "game" systems (`PlayerInputSystem`, `CharacterMotorSystem`,
`CharacterStateSystem`, `CharacterInputSystem`, `PlatformBuilderSystem`) do not
belong in the engine. `CameraSystem` is ambiguous and should be split.

---

## 4. Concrete Path Forward

### Phase 1 — Module convention (do this now, in-repo)

The most impactful near-term change is not a directory split. It is a
**module registration convention** that makes `main.cpp` a wiring manifest
instead of a soup of individual setup calls.

Currently `main.cpp` does:

```cpp
AssetResource assets; assets.load(); world.set_resource(assets);
AudioResource audio;  audio.load();  world.set_resource(audio);
world.set_resource(EventRegistry{});
auto& reg = world.resource<EventRegistry>();
reg.register_queue<JumpEvent>(world);
reg.register_queue<LandEvent>(world);
PhysicsSystem::Register(world);
CharacterMotorSystem::Register(world);
// ... pipeline lambdas spread across the file ...
```

With a module convention it becomes:

```cpp
EngineModules::install(world, pipeline);   // physics, render, audio, input, debug
GameModules::install(world, pipeline);     // character, camera, builder
```

Each module is a `struct` or `namespace` with a single `install(World&,
Pipeline&)` function that handles resource setup, event queue registration,
system registration, and pipeline wiring — all in one place, in the file that
owns the system.

This is the Bevy plugin pattern and the Flecs module pattern. It does not
require ECS changes. It is a naming and organisation convention.

**Concrete form for this project:**

```cpp
// src/engine/audio_module.hpp
struct AudioModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        InitAudioDevice();
        AudioResource audio; audio.load(); world.set_resource(std::move(audio));
        world.resource<EventRegistry>().register_queue<JumpEvent>(world);
        world.resource<EventRegistry>().register_queue<LandEvent>(world);
        pipeline.add_logic([](ecs::World& w, float dt) { AudioSystem::Update(w, dt); });
    }
    static void shutdown(ecs::World& world) {
        world.resource<AudioResource>().unload();
        CloseAudioDevice();
    }
};
```

`main.cpp` shrinks to startup ordering, a game loop, and `install`/`shutdown`
calls. It becomes a wiring manifest, not an implementation file.

**This should be the next RFC (RFC-0013).**

### Phase 2 — Directory split (do when a second game begins)

Once a second game is started, create the directory boundary:

```
src/
  engine/     ← everything in the "Engine" column of the boundary table
  game/       ← everything in the "Game" column
main.cpp      ← wiring only
```

CMake:
```cmake
add_library(engine STATIC
    src/engine/systems/physics.cpp
    src/engine/systems/renderer.cpp
    src/engine/systems/audio.cpp
    ...
)
target_include_directories(engine PUBLIC src/engine)

add_executable(my_game main.cpp src/game/systems/character_motor.cpp ...)
target_link_libraries(my_game PRIVATE engine ecs Jolt raylib ...)
```

The `engine` target has no `#include` paths into `src/game/`. Game code can
see `src/engine/`. This is enforced by the build system, not by discipline.

**Do not do this now.** The boundary is still shifting. The `CameraSystem`
ambiguity, the question of whether `PlayerInputSystem` is engine or game, the
future of character generalisation — these are all unresolved. Locking a
directory structure prematurely makes refactoring harder, not easier.

### Phase 3 — Extract engine repo (do when three or more games share it)

When there are enough game projects to demonstrate that the engine is genuinely
shared and stable:

1. Move `src/engine/` to its own git repository.
2. Existing and new games `git submodule add` it, exactly as they do the ECS.
3. Engine improvements go through the engine repo's own RFC/test process
   (which already exists — the RFC workflow applies).
4. Game repos pin a specific engine commit and upgrade intentionally.

This mirrors the ECS relationship precisely. The ECS is small and stable so
this works well. The engine layer is larger, which means the RFC/versioning
discipline matters more.

**Do not do this now.** Premature extraction of a single-game codebase into a
shared library is one of the most common sources of wasted engineering effort
in game development.

---

## 5. Easy Extension — The Concrete Mechanism

The question "how do we easily add new systems?" has a specific answer once
the module convention is in place:

**Adding a new engine system:**
1. Create `src/engine/systems/my_system.hpp` and `.cpp`.
2. Add `MySystem::Update` to the relevant module's `install()`.
3. If it needs new event types, add them to `events.hpp` and register in `install()`.
4. Write tests in `tests/`.
5. Done — `main.cpp` is untouched because `EngineModules::install` calls it.

**Adding a new game system:**
1. Create `src/game/systems/my_game_system.hpp` and `.cpp`.
2. Add it to `GameModules::install()`.
3. Done.

**Adding a new game (Phase 3):**
1. Create new repo, `git submodule add engine`.
2. Write `main.cpp` that calls `EngineModules::install(world, pipeline)`.
3. Write game systems.
4. Load a game-specific JSON scene.
5. Done — the engine is intact, only the game layer is new.

The module convention is the key enabling mechanism. Without it, "main.cpp as
wiring manifest" is aspirational. With it, the wiring is co-located with the
system it wires, and main.cpp becomes a five-line ordering statement.

---

## 6. What Not To Do

**Do not introduce virtual interfaces at the engine/game boundary.** A
`class IPhysicsSystem { virtual void Update() = 0; };` abstraction provides
mock-ability at the cost of virtual dispatch on every physics frame. The ECS
data model already provides the decoupling you need — game code reads component
data, not a pointer to a physics object. Interfaces are the right tool for
plugin architectures (mods, DLLs); they are the wrong tool for a statically-
linked in-house engine.

**Do not fork.** This project's entire value proposition is a reusable engine.
A fork abandons that immediately.

**Do not split directories before the boundary is stable.** Moving files is
cheap; moving a directory structure that was wrong is expensive because every
include path must be updated. Wait until the game/engine boundary is clear
from use.

**Do not create a "game API" with versioned stability guarantees prematurely.**
There is no second game yet. An API that nobody depends on has no reason to be
stable. Stability comes from proven usage patterns, not from planning.

---

## 7. Summary

| Question | Answer |
|----------|--------|
| Does the directory split have runtime overhead? | No. Zero. |
| Fork or shared? | Shared submodule (same pattern as ECS). |
| When to split directories? | When the second game starts. |
| When to extract engine repo? | When three or more games share it. |
| What is the most impactful change now? | Module convention (`install(world, pipeline)`). |
| What makes extension easy? | Module convention + RFC discipline already in place. |

The engine/game boundary this project needs already exists conceptually
(documented in `engine-game-separation.md`). The next concrete step is to
make it visible in code via the module convention — an RFC, not a refactor.
Everything else follows from that.
