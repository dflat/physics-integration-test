# Engine / Game Separation — Architectural Considerations

*A design memo for the physics-integration-test project, February 2026.*

---

## 1. Where We Stand

The project is deliberately being built as an **engine first, game second**.
The current prototype — a parkour character on a physics-driven platformer level
— exists to exercise the engine's systems, not to ship a game. This is a sound
instinct, but "engine vs game" is one of the oldest and most nuanced tensions in
game development. This document surveys the landscape honestly: what the
industry has learned, what the real tradeoffs are, and what paths make sense for
this project.

---

## 2. The Core Tension

An engine is a reusable substrate. A game is a specific expression of that
substrate. The tension arises because the two have opposing pressures:

| Engine pressure | Game pressure |
|-----------------|---------------|
| General — works for many games | Specific — solves this game's problems |
| Stable API contracts | Rapid iteration, frequent change |
| Minimal assumptions about game rules | Deep integration with game rules for performance |
| Testable in isolation | Validated by playing |
| Long-lived | Often disposable post-ship |

The failure modes are symmetric. An engine that absorbs too much game logic
becomes a single-game framework that cannot be reused. A game that refuses to
commit anything to the engine layer ends up re-implementing physics, rendering,
and input in gameplay code, which is slower, buggier, and harder to reason about.

---

## 3. Industry Patterns

### 3.1 The Monolith (no separation)

Most shipped games. The "engine" is the game; all code is coupled. Common in
solo projects, game jams, studios that ship one game per decade. The Unity
asset store model encourages this — everything is a MonoBehaviour.

**Advantages:** Zero overhead from abstraction. Fastest path to a working game.
**Disadvantages:** Cannot be reused. Maintenance cost grows super-linearly.
Changing a core system (physics, rendering) is a full rewrite.

### 3.2 The Licensed Engine (full decoupling via commercial product)

Unreal, Unity, Godot. The engine is an external product; the game is a plugin
or project within it. The boundary is enforced by the engine vendor.

**Advantages:** Enormous reuse. Large ecosystem. Battle-tested.
**Disadvantages:** You own none of the substrate. The abstraction boundary is
fixed by someone else's API. Performance escape hatches (custom memory, SIMD,
render pipelines) are expensive or impossible.

### 3.3 The In-House Engine (explicit layer separation)

Studios like Naughty Dog (Decima/in-house), CDPR (REDengine), id Software
(idTech). The engine is a distinct product; games are built on top of it.
Separation is enforced by team structure (an engine team, a game team) and
module boundaries.

**Advantages:** Full control. Can be reused across titles. Can be optimised
deeply without external constraints.
**Disadvantages:** Extremely expensive to build correctly. The engine team must
anticipate game needs without building game-specific code. This is hard — most
in-house engines have leaked game assumptions that make reuse difficult.

### 3.4 The ECS Boundary (data-oriented separation)

The pattern this project is already using. ECS naturally separates concerns:

- **Components** are data — engine-agnostic by definition (if kept pure).
- **Systems** are logic — can be engine-only, game-only, or mixed.
- **Queries** are the coupling mechanism — a system expresses what data it
  needs; it does not depend on other systems directly.

This is the model used by Bevy (Rust), Flecs (C/C++), and Unity's DOTS layer.
The boundary between "engine system" and "game system" is a naming and
organizational convention, not a runtime mechanism.

**The critical insight:** in a pure ECS, **the engine/game boundary is not
enforced by the type system or the runtime — it is enforced by discipline**.
The question is what discipline looks like in practice.

---

## 4. Coupling Strategies

### 4.1 Structural Coupling (what we have now)

All systems live in the same binary, the same `src/` tree, the same build
target. The separation is conceptual: some systems are "engine" (physics,
rendering, input, audio), some are "game" (character, builder, camera).

- **Boundary enforcement:** None. Any file can include any other.
- **Cost to cross:** Zero. A "game" system can include an "engine" header freely.
- **Reuse mechanism:** By convention and discipline.
- **Current state:** `PlatformBuilderSystem`, `CharacterMotorSystem` are game
  systems living in `src/systems/` next to engine systems.

### 4.2 Module / Directory Coupling

A common first step: split `src/` into `src/engine/` and `src/game/`. Engine
code has no include paths into `src/game/`. Game code can see `src/engine/`.
Enforced by CMake `target_include_directories` on separate targets.

```
engine (static lib): src/engine/
  systems: physics, rendering, audio, input, debug
  headers: components, events, debug_panel, asset/audio resources

game (executable or shared lib): src/game/
  systems: character_motor, character_state, platform_builder, camera
  scene files, game-specific components
```

- **Boundary enforcement:** Weak (CMake can express it, but is often bypassed).
- **Cost to cross:** A CMake dependency change. Low friction.
- **Reuse mechanism:** Ship `engine/` as a static library or submodule.
- **When it helps:** Multiple game projects using the same engine codebase.

### 4.3 Interface / API Coupling

The engine exposes a stable API: a set of headers, component types, and system
registration points. Game code depends only on this API, not on engine
internals. The engine can change its internals without breaking game code.

This is how Unreal's `GameFramework` module works: `ACharacter`, `APlayerController`,
etc. are stable base classes; the engine's renderer and physics are opaque.

In an ECS context, the "API" is:
- The set of component types the engine defines and owns
- The events the engine emits
- The pipeline phases available to game systems

- **Boundary enforcement:** Strong (only API headers are published).
- **Cost to cross:** Requires engine changes and versioning.
- **Reuse mechanism:** The engine is a versioned library with a changelog.
- **When it helps:** Multiple teams; long engine lifetime across many games.

### 4.4 Data-Only Coupling (the ECS ideal)

The purest ECS approach: engine systems and game systems communicate
exclusively through **components and events**. No system imports another system's
header. The engine doesn't know game component types exist; the game doesn't
know engine system internals.

```
Engine system:    reads Transform, writes PhysicsState
Game system:      reads PhysicsState, writes CharacterIntent
No direct call:   CharacterMotor doesn't call PhysicsSystem; it reads data Physics wrote
```

This is the ideal, but it has costs: it requires more components (data buses
between systems), more event types, and discipline to never reach across the
boundary via a direct include.

- **Boundary enforcement:** Very strong (no cross-system includes possible).
- **Cost to cross:** High (new component or event type required).
- **Reuse mechanism:** Swap engine systems without touching game code.
- **When it helps:** Very large teams; plugin/mod architectures.

---

## 5. The Real Tradeoffs

### Premature decoupling is a real cost

Every abstraction layer is a tax. An interface between engine and game that is
designed before you know what the game needs will be wrong. You will either
over-engineer it (abstracting things that never change) or under-engineer it
(missing the seams that actually matter). The right abstraction reveals itself
through use, not through planning.

**The risk for this project:** over-investing in engine/game separation before
the engine has enough surface area to know where the seams belong.

### Under-decoupling is also a real cost

`PlatformBuilderSystem` in `src/systems/` next to `physics.cpp` is fine today.
It becomes a problem if:
1. You want to compile a second game without the platform builder.
2. You want to replace the physics engine without touching builder logic.
3. A new developer assumes everything in `src/systems/` is reusable engine code.

The cost of under-decoupling is discovered late — when you try to reuse and
can't.

### The key question: what is the reuse unit?

The answer determines the right boundary:

- **Reuse the whole engine (all systems):** Module separation is enough.
  Ship `src/engine/` as a library; game is a thin layer on top.
- **Reuse some systems (physics + rendering but not character):** Interface
  coupling is needed. The character system must not leak into the engine.
- **Reuse only the ECS substrate:** No coupling strategy matters; each game
  reimplements its own systems on top of the bare ECS.

For this project, the likely answer is: **reuse physics + rendering + input +
audio + the event bus + the scene loader**. Character, camera, and game-specific
builders are game code.

---

## 6. State of the Art (2026)

**Bevy (Rust):** Plugins as the boundary mechanism. Engine is a collection of
plugins (`PhysicsPlugin`, `RenderPlugin`). Game code adds its own plugins.
Plugin dependency declarations make the boundary explicit. No shared global
state — everything is typed resources and events. The cleanest ECS separation
currently in production use.

**Flecs (C/C++):** Module system. Systems and components are grouped into named
modules; modules can declare dependencies on other modules. An engine module
cannot see game module internals. Very similar to the directory-coupling
approach but enforced by the ECS runtime.

**Unity DOTS:** "Worlds" as the boundary. Engine systems run in the main World;
game systems can run in separate sub-Worlds or the same World. Thin on
enforcement; relies on team discipline. The `ISystem` / `SystemBase` split is
mostly a performance concern, not a coupling concern.

**id Tech / REDengine tradition:** Strict binary separation. Engine ships as
a DLL/SO; game loads it at runtime. Allows hot-swapping the engine during
development. Expensive to maintain the ABI boundary; almost no indie teams do
this.

---

## 7. Recommended Path for This Project

Given the goals (engine reuse across games, prototype-first development, small
team, no ABI stability requirement), the pragmatic path is:

### Near-term: Directory separation

Split `src/` into `src/engine/` and `src/game/` when we have a second system
that is clearly game-specific. The platform builder is already there. When the
character system grows game-specific logic (abilities, stats, game state), move
it too.

Enforce with CMake: `engine` as a static library target with no includes from
`src/game/`. `game` links `engine` and adds its own systems.

No interface abstraction yet — that requires knowing the stable surface, which
requires more games.

### Medium-term: Plugin/module convention

Adopt the Bevy/Flecs pattern of grouping systems into cohesive modules with a
single registration point. An `AudioModule::register(world, pipeline)` registers
the audio resource, registers event queues, and adds the system to the pipeline
in one call — instead of the current three separate steps in `main.cpp`.

This is the `Register()`-as-general-init pattern we discussed and deferred.
It cleans up `main.cpp` and makes the engine/game boundary explicit in code
without requiring ABI stability.

### Long-term: Event-only coupling (if needed)

If a second game needs physics and rendering but completely different gameplay
systems, invest in a strict event-only boundary: engine systems emit and consume
typed events; game systems do the same; no system imports another system's
header. This is the highest-cost, highest-flexibility option — appropriate when
the reuse pattern is proven and the pain of crossing the boundary is felt.

### What not to do

- Don't create a formal "engine API" before you know what two games need in
  common. The API will be wrong.
- Don't couple game logic to engine internals (e.g., calling Jolt directly from
  the platform builder). Keep game systems reading ECS data only.
- Don't delay the directory split indefinitely — the longer engine and game code
  share a flat directory, the harder it is to separate later.

---

## 8. Our Current Boundary (Honest Assessment)

| System | Engine or Game? | Coupling concern |
|--------|-----------------|------------------|
| `PhysicsSystem` | Engine | Clean — no game deps |
| `RenderSystem` | Engine | Clean — reads standard components |
| `InputGatherSystem` | Engine | Clean |
| `PlayerInputSystem` | Game | Currently in `src/systems/` — misleading |
| `AudioSystem` | Engine | Clean — event-driven, no game knowledge |
| `DebugSystem` | Engine | Clean |
| `SceneLoader` | Engine | Clean |
| `CameraSystem` | Game/Engine | Ambiguous — orbit camera is general; follow-mode logic is game-specific |
| `CharacterMotorSystem` | Game | In `src/systems/` — should move to `src/game/` |
| `CharacterStateSystem` | Game | Same |
| `CharacterInputSystem` | Game | Same |
| `PlatformBuilderSystem` | Game | Clearly game code; misplaced today |

The immediate actionable step is to create `src/game/` and move the five
clearly-game systems there. That's an RFC in itself — one that also resolves
the `CameraSystem` ambiguity (split the orbit math into the engine, leave the
follow-mode binding in game code).

---

*This document is a living record of architectural thinking, not a binding
decision. Update it as the project evolves.*
