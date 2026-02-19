# ARCH-0006: Character System

* **RFC Reference:** [RFC-0006 — Character System Decomposition](../rfcs/02-implemented/0006-character-system-decomposition.md)
* **Implementation Date:** 2026-02-19
* **Status:** Active

---

## 1. High-Level Mental Model

> The character system is a three-stage pipeline: it translates raw hardware input
> into world-space intent, runs a state machine to determine what the physics layer
> is *allowed* to do this frame, and finally drives the Jolt character controller.
> Each stage is a separate system that communicates with the next exclusively through
> ECS components.

* **Core Responsibility:** Move a player-controlled capsule through a Jolt physics
  world with coyote-time double-jump mechanics.
* **Pipeline Phase:** All three systems run in the **Logic phase**, in strict order,
  immediately before the fixed-step Physics phase. The motor must be the last Logic
  system so velocity commands reach Jolt before `ExtendedUpdate` is called.
* **Key Constraints:** `CharacterMotorSystem` must execute before `PhysicsSystem`
  or velocity commands are silently dropped for one frame. The three `Register()`
  calls must happen before `SpawnScene()` so lifecycle hooks are in place when
  `CharacterControllerConfig` is first added to the player entity.

---

## 2. The Grand Tour

| File | Role |
|------|------|
| `src/systems/character_input.hpp/cpp` | Stage 1 — projects 2D hardware input onto world-space `CharacterIntent`. |
| `src/systems/character_state.hpp/cpp` | Stage 2 — state machine (ground, air, coyote, jump count). Exposes `apply_state()` for unit testing. |
| `src/systems/character_motor.hpp/cpp` | Stage 3 — applies velocity, rotation, `ExtendedUpdate`, and syncs transforms back to ECS. Also owns the Jolt `CharacterVirtual` lifecycle. |
| `src/components.hpp` | Defines `CharacterControllerConfig`, `CharacterHandle`, `CharacterIntent`, and `CharacterState`. |

### Lifecycle vs. Per-Frame Work

**`Register(World&)`** — called once at startup from `main.cpp`, before `SpawnScene`.

Each of the three systems registers an `on_add<CharacterControllerConfig>` hook.
When `CharacterControllerConfig` is added to an entity (in `SpawnScene`), all three
hooks fire in registration order:

1. `CharacterInputSystem::Register` → adds `CharacterIntent{}` to the entity.
2. `CharacterStateSystem::Register` → adds `CharacterState{}` to the entity.
3. `CharacterMotorSystem::Register` → creates the `JPH::CharacterVirtual` from the
   config's mass, height, and radius, then adds `CharacterHandle{character}`.

The Jolt character is never created manually — it is always a consequence of adding
`CharacterControllerConfig`. This mirrors the pattern used by `PhysicsSystem` for
rigid bodies.

**`Update(World&, float dt)`** — called every Logic phase tick:

- `CharacterInputSystem::Update`: queries `PlayerTag + PlayerInput + CharacterIntent`.
  Reads `view_forward` / `view_right` from `PlayerInput` (written by `CameraSystem`
  in the same phase, one slot earlier). Projects the 2D `move_input` stick values
  onto the world-space xz-plane and writes the resulting `CharacterIntent`.

- `CharacterStateSystem::Update`: queries `CharacterHandle + CharacterIntent +
  CharacterState`. Calls `character->GetGroundState()` once, then delegates to
  `apply_state()` — the pure, testable core of the state machine.

- `CharacterMotorSystem::Update`: queries `CharacterHandle + CharacterIntent +
  CharacterState + WorldTransform`. Computes horizontal and vertical velocity,
  applies rotation, calls `ExtendedUpdate`, and syncs the character's new Jolt
  position back to `LocalTransform` / `WorldTransform`.

---

## 3. Components & Data Ownership

| Component | Owner (writer) | Consumers (readers) | Notes |
|-----------|---------------|---------------------|-------|
| `CharacterControllerConfig` | `main.cpp` / `SpawnScene` | All three `Register` hooks | Authoring data; triggers the full lifecycle chain |
| `CharacterIntent` | `CharacterInputSystem::Update` | `CharacterStateSystem`, `CharacterMotorSystem` | Reset and rewritten every frame |
| `CharacterState` | `CharacterStateSystem::Update` | `CharacterMotorSystem` | `jump_impulse` is the key handoff field |
| `CharacterHandle` | `CharacterMotorSystem::Register` hook | `CharacterStateSystem`, `CharacterMotorSystem` | Holds `shared_ptr<JPH::CharacterVirtual>`; created once, lives until entity is destroyed |
| `PlayerInput` | `PlayerInputSystem`, `CameraSystem` | `CharacterInputSystem` | `view_forward` / `view_right` are written by `CameraSystem` — a coupling that RFC-0007 will resolve |
| `PlayerState` | `PlatformBuilderSystem` | `PlatformBuilderSystem` | Builder-only; the character system does not touch this |

**Deferred Commands:** None. The character system does not create or destroy entities.
`CharacterIntent` and `CharacterState` are added inside `on_add` hooks (not deferred),
which is safe because hooks fire synchronously on the `world.add()` call site before
the entity is visible to queries.

**Resources read:** `std::shared_ptr<PhysicsContext>` — accessed in
`CharacterMotorSystem::Register` to obtain the Jolt physics system pointer needed to
construct `CharacterVirtual`, and in `Update` for the broadphase/layer filter objects
passed to `ExtendedUpdate`.

---

## 4. Data Flow

```
PlayerInputSystem (Pre-Update)
    writes: PlayerInput.move_input, .jump, etc.
        │
CameraSystem (Logic — slot 1)
    writes: PlayerInput.view_forward, PlayerInput.view_right
        │
CharacterInputSystem (Logic — slot 2)
    reads:  PlayerInput (move_input, jump, view_forward, view_right)
    writes: CharacterIntent (move_dir, look_dir, jump_requested)
        │
CharacterStateSystem (Logic — slot 3)
    reads:  CharacterHandle → GetGroundState()
            CharacterIntent.jump_requested
    writes: CharacterState (mode, jump_count, air_time, jump_impulse)
        │
CharacterMotorSystem (Logic — slot 5, last)
    reads:  CharacterIntent.move_dir
            CharacterState.mode, .jump_impulse
            CharacterHandle → GetLinearVelocity()
    writes: CharacterHandle → SetLinearVelocity(), SetRotation(), ExtendedUpdate()
            LocalTransform.position, .rotation
            WorldTransform.matrix
        │
PhysicsSystem (Physics — fixed 60 Hz)
    steps rigid bodies; character transform already synced above
```

---

## 5. System Integration (The Social Map)

**Upstream — must run before this subsystem:**
- `InputGatherSystem` (Pre-Update) — populates `InputRecord`.
- `PlayerInputSystem` (Pre-Update) — maps `InputRecord` → `PlayerInput`.
- `CameraSystem` (Logic, slot 1) — writes `view_forward` / `view_right` into
  `PlayerInput`. `CharacterInputSystem` reads these in slot 2; if Camera runs
  after CharacterInput the character will move with one-frame-stale view directions.

**Downstream — must run after this subsystem:**
- `PhysicsSystem` (Physics phase) — steps the Jolt world. `CharacterMotorSystem`
  must have committed velocity commands before this runs.
- `RenderSystem` (Render phase) — reads `WorldTransform`, which `CharacterMotorSystem`
  has already synced.

**Ordering constraint:**
```
CameraSystem → CharacterInputSystem → CharacterStateSystem
    → PlatformBuilderSystem → CharacterMotorSystem → [PhysicsSystem]
```
`PlatformBuilderSystem` is slotted between `CharacterStateSystem` and
`CharacterMotorSystem` because it is data-independent from the character pipeline
and its deferred spawns need to be queued before the Logic-phase flush.

**Note on transform sync:** Rigid body transforms are synced by `PhysicsSystem`
after each fixed step. Character transforms are synced by `CharacterMotorSystem`
after `ExtendedUpdate`. This asymmetry exists because `CharacterVirtual` is not a
standard Jolt body — it does not participate in the body interface's bulk transform
sync.

---

## 6. Trade-offs & Gotchas

**Why three systems, not two?**
The state machine (`CharacterStateSystem`) is intentionally isolated from both input
and physics so that its transitions can be tested without constructing a Jolt world
or a Raylib window. `CharacterStateSystem::apply_state()` is a public static method
that takes only primitives and POD structs — no engine context required. Unit tests
for it are deferred to RFC-0007, which removes Raylib and Jolt from `components.hpp`
and makes the test target headless-capable.

**`jump_impulse` as a one-frame signal:**
`CharacterState::jump_impulse` is reset to `0.0f` at the start of every
`apply_state()` call. It is set to a non-zero value only during the frame a jump
fires. `CharacterMotorSystem` reads it and applies it as the Y component of the new
velocity. If you add a system between `CharacterStateSystem` and `CharacterMotorSystem`
that also writes to `CharacterState`, be careful not to clobber `jump_impulse` before
the motor consumes it.

**Coyote time is currently redundant:**
The eligibility check is `on_ground || can_coyote || jump_count < 2`. Because
`jump_count < 2` is always true when `jump_count == 0`, the coyote window does not
actually gate the first jump — it can always fire as long as a jump slot is available.
The coyote condition is present for future intent (if the double-jump slot is ever
removed or conditioned differently). Do not "simplify" it away without understanding
this context.

**View direction coupling (pending RFC-0007):**
`CameraSystem` currently writes `view_forward` / `view_right` into `PlayerInput`,
which `CharacterInputSystem` then reads. This is a bidirectional coupling on a
component that should be input-only. RFC-0007 moves these fields to the `MainCamera`
resource. Until then, `CharacterInputSystem` reads from `PlayerInput`, and
`CameraSystem` must always run before `CharacterInputSystem` in the Logic phase.

**Registration order is hook-firing order:**
The three `Register()` calls in `main.cpp` happen in this order:
```cpp
CharacterInputSystem::Register(world);   // hook 1: adds CharacterIntent
CharacterStateSystem::Register(world);   // hook 2: adds CharacterState
CharacterMotorSystem::Register(world);   // hook 3: creates CharacterVirtual, adds CharacterHandle
```
All three hooks fire when `CharacterControllerConfig` is added in `SpawnScene`.
`CharacterMotorSystem`'s hook must be last because it reads `LocalTransform` (for
initial position) and adds `CharacterHandle`. If the order is swapped, the Jolt
character may be created before its initial transform is set.

---

## 7. Related Resources

* **RFC:** [`docs/rfcs/02-implemented/0006-character-system-decomposition.md`](../rfcs/02-implemented/0006-character-system-decomposition.md)
* **Prior thought doc:** [`docs/thoughts/0006-refactor-character-system.md`](../thoughts/0006-refactor-character-system.md)
* **Components:** `src/components.hpp` — search `CharacterIntent`, `CharacterState`, `CharacterHandle`, `CharacterControllerConfig`
* **Tests:** `apply_state()` unit tests deferred to RFC-0007 (`tests/logic_tests.cpp`, tag `[character]`)
* **ARCH_STATE.md:** Section 3 (System Responsibilities table) for a one-line summary of each system
