# RFC-0009: Frame-Scoped Event Bus

* **Status:** Implemented
* **Date:** February 2026
* **Depends on:** RFC-0008 (Physics Handle Isolation — `components.hpp` is now
  engine-free, so `events.hpp` can also be zero-dependency and testable headlessly)

## Summary

Introduce a typed, frame-scoped event bus. Systems emit strongly-typed events
into per-type queues stored as World resources. All queues are cleared once at
the start of each frame, so events are visible to every system that runs after
the emitter in the same frame and are gone the next frame. The first concrete
events are `JumpEvent` and `LandEvent`, emitted by `CharacterStateSystem`.

## Motivation

All inter-system communication currently goes through component data. This is
correct for persistent state (`CharacterState`, `MainCamera`, etc.) but awkward
for transient signals — things that happen once and should notify other systems
without leaving a residue:

- **Jump** — audio should play a sound, a particle system should spawn dust, a
  camera shake system should apply an impulse. None of these should have to poll
  `CharacterState.jump_impulse` each frame and track whether they already reacted.
- **Land** — same pattern: audio, particles, screen shake.
- **Platform spawned** — future analytics or audio.
- **Scene reset** — future audio fade-out, transition effects.

Without events, adding a second consumer of "the player just jumped" means
either coupling CharacterStateSystem to that consumer directly, or
introducing a one-frame boolean flag in a component that every consumer must
reset. Events are the right abstraction for transient, many-consumer signals.

## Design

### `Events<T>` — per-type queue

```cpp
template<typename T>
struct Events {
    void send(T event);
    const std::vector<T>& read() const;
    bool empty() const;
    void clear();
};
```

Each event type has its own `Events<T>` resource registered in the World.
Systems call `world.resource<Events<JumpEvent>>().send(...)` to emit and
`world.resource<Events<JumpEvent>>().read()` to consume. The queue is a plain
`std::vector<T>` — no heap allocation beyond the vector itself.

### `EventRegistry` — frame flush

```cpp
class EventRegistry {
public:
    template<typename T>
    void register_queue(ecs::World& world); // creates Events<T> resource + stores flush fn

    void flush_all();
};
```

`EventRegistry` is stored as a World resource. During startup, each event type
is registered via `register_queue<T>()`, which:
1. Calls `world.set_resource(Events<T>{})`.
2. Stores a `std::function<void()>` that calls `world.try_resource<Events<T>>()->clear()`.

The pipeline calls `flush_all()` as its first Pre-Update step each frame,
before any system runs. This gives every phase of the frame a clean read of
events emitted during that same frame.

### Lifetime semantics

```
Frame N:
  Pre-Update:
    [0] EventRegistry::flush_all()  ← clears everything from frame N-1
    [1] InputGatherSystem
    [2] PlayerInputSystem
  Logic:
    CharacterStateSystem emits JumpEvent / LandEvent
    (future) AudioSystem reads JumpEvent, plays sfx
  Physics: ...
  Render:
    (future) EffectsSystem reads LandEvent, draws dust

Frame N+1:
  Pre-Update:
    [0] flush_all() ← JumpEvent and LandEvent from frame N are now gone
```

Events emitted in frame N are readable by every system that runs after the
emitter in frame N. They are gone at the start of frame N+1.

### Initial concrete events

```cpp
// Emitted by CharacterStateSystem when a jump fires (jump_impulse > 0).
struct JumpEvent {
    ecs::Entity entity;
    int         jump_number; // 1 = first jump, 2 = double jump
    float       impulse;     // velocity applied (m/s)
};

// Emitted by CharacterStateSystem on Airborne → Grounded transition.
struct LandEvent {
    ecs::Entity entity;
};
```

### CharacterStateSystem emission

`apply_state` stays pure — no event knowledge. `Update` wraps the call,
captures the prev mode and post-apply jump_impulse, then emits:

```cpp
void CharacterStateSystem::Update(World& world, float dt) {
    auto* jump_ev = world.try_resource<Events<JumpEvent>>();
    auto* land_ev = world.try_resource<Events<LandEvent>>();

    world.each<CharacterHandle, CharacterIntent, CharacterState>(
        [&, dt](Entity e, CharacterHandle& h, CharacterIntent& intent, CharacterState& state) {
            CharacterState::Mode prev_mode = state.mode;
            bool on_ground = ...; // GetGroundState() call
            apply_state(on_ground, dt, intent, state);

            if (state.jump_impulse > 0.0f && jump_ev)
                jump_ev->send({e, state.jump_count, state.jump_impulse});

            if (prev_mode == CharacterState::Mode::Airborne &&
                state.mode == CharacterState::Mode::Grounded && land_ev)
                land_ev->send({e});
        });
}
```

The `try_resource` guards mean the system works correctly even if no event
queues have been registered — no assertions, no crashes.

### Startup wiring (main.cpp)

```cpp
// Register all event types — creates the resources and stores flush callbacks.
auto& reg = world.resource<EventRegistry>();
reg.register_queue<JumpEvent>(world);
reg.register_queue<LandEvent>(world);

// First pre_update: flush previous frame's events.
pipeline.add_pre_update([](ecs::World& w, float) {
    w.resource<EventRegistry>().flush_all();
});
// Then add the real input systems...
```

`EventRegistry` is set as a resource before `Register` calls and before the
pipeline is constructed.

### Files Changed

| Action | Path | Reason |
|--------|------|--------|
| Create | `src/events.hpp` | `Events<T>`, `EventRegistry`, `JumpEvent`, `LandEvent` |
| Modify | `src/systems/character_state.cpp` | Emit `JumpEvent` / `LandEvent` in `Update` |
| Modify | `src/main.cpp` | Register event queues, add flush as first pre_update |
| Modify | `tests/logic_tests.cpp` | Unit tests for `Events<T>` API |

## Alternatives Considered

**Signal/slot callbacks (observer pattern).** Systems register handlers for
event types at setup time; the emitter calls all handlers synchronously.
Rejected: introduces hidden execution order (callbacks fire mid-emitter),
makes it hard to reason about which systems ran when, and is harder to test.

**ECS event entities** (entities with event components, tagged for
one-frame destruction). Rejected: entity creation overhead, messy lifetime
via age tags, requires a dedicated cleanup system.

**Persistent event log (never cleared).** Systems track their read cursor.
Rejected: unbounded growth, more complex consumer code.

**Single global event type (tagged union / `std::any`).** Rejected: loses
type safety, requires casting at every read site.

## Testing

- Unit tests for `Events<T>`: `send`/`read`/`clear`/`empty` behavior; clearing
  does not affect an empty queue; multiple sends accumulate.
- `events.hpp` has zero engine-library dependencies — verifiable by including
  it in the headless test target.
- Functional (manual): jump and observe that future audio/effect systems in
  later RFCs react exactly once per jump, not continuously.
