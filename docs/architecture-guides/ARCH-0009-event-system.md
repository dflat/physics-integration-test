# ARCH-0009: Frame-Scoped Event Bus

* **RFC Reference:** [RFC-0009 — Frame-Scoped Event Bus](../rfcs/02-implemented/0009-event-system.md)
* **Implementation Date:** 2026-02-19
* **Status:** Active

---

## 1. High-Level Mental Model

> Components model **persistent state** — things that exist across many frames.
> Events model **transient signals** — things that happen once and should notify
> any system that cares, with zero residue the next frame.
>
> The bus is one world resource per event type (`Events<T>`), cleared at the
> start of every frame by `EventRegistry::flush_all()`.

* **Core Responsibility:** Decouple emitters from consumers for one-shot
  per-frame signals (jumps, landings, spawns, resets).
* **Pipeline Phase:** `EventRegistry::flush_all()` runs as the first Pre-Update
  step. Events are readable by all phases of the same frame, gone the next.
* **Key Constraint:** Do not emit the same logical event from multiple places.
  One writer per event type. Many readers are fine.

---

## 2. The Grand Tour

| File | Role |
|------|------|
| `src/events.hpp` | `Events<T>` template, `EventRegistry`, `JumpEvent`, `LandEvent`. Zero engine-library dependencies — includable in headless tests. |
| `src/systems/character_state.cpp` | Emitter — detects jump and land transitions in `Update`, emits to the queues via `try_resource` guards. |
| `src/main.cpp` | Wires everything — creates `EventRegistry` resource, registers both queue types, adds flush as the first Pre-Update lambda. |
| `tests/logic_tests.cpp` | `Events<T>` API unit tests (send, read, clear, accumulation order). |

### Lifecycle vs. Per-Frame Work

**Lifecycle (startup, in `main.cpp`):**
```cpp
world.set_resource(EventRegistry{});
auto& reg = world.resource<EventRegistry>();
reg.register_queue<JumpEvent>(world);  // creates Events<JumpEvent> resource
reg.register_queue<LandEvent>(world);  // creates Events<LandEvent> resource
```

`register_queue<T>()` does two things atomically: stores `Events<T>{}` as a
world resource, and pushes a flush lambda into `EventRegistry::flush_fns_`.

**Per-Frame (pipeline):**
1. `EventRegistry::flush_all()` — first Pre-Update step, clears all queues.
2. Systems run — emitters call `send()`, consumers call `read()`.
3. (Repeat next frame.)

---

## 3. Components & Data Ownership

### `Events<T>` — the queue

```cpp
template<typename T>
struct Events {
    void send(T event);               // emit
    const std::vector<T>& read();     // consume (read-only view)
    bool empty() const;
    void clear();                     // called by EventRegistry, not by systems
};
```

Stored as a world resource. Any system accesses it via:
```cpp
auto* ev = world.try_resource<Events<JumpEvent>>();
if (ev) { /* emit or read */ }
```

**Never call `clear()` from a system.** Clearing is owned exclusively by
`EventRegistry::flush_all()`. A system that clears its own queue would steal
events from consumers that run later in the same frame.

### Current event types

| Event | Fields | Emitter | Consumers (current/planned) |
|-------|--------|---------|------------------------------|
| `JumpEvent` | `entity`, `jump_number` (1 or 2), `impulse` (m/s) | `CharacterStateSystem::Update` | future: Audio, Particles |
| `LandEvent` | `entity` | `CharacterStateSystem::Update` | future: Audio, Camera shake |

---

## 4. Data Flow

```
Frame N:

  Pre-Update:
    flush_all()          ← Events<JumpEvent>, Events<LandEvent> cleared
    InputGatherSystem
    PlayerInputSystem

  Logic:
    CameraSystem
    CharacterInputSystem
    CharacterStateSystem  ← apply_state() → detects jump / land
                            jump_ev->send({e, jump_count, impulse})
                            land_ev->send({e})
    PlatformBuilderSystem
    CharacterMotorSystem

  Physics + Render:
    (future AudioSystem reads Events<JumpEvent>.read() here)
    (future EffectsSystem reads Events<LandEvent>.read() here)

Frame N+1:
    flush_all()          ← events from frame N are gone
```

---

## 5. System Integration (The Social Map)

**`CharacterStateSystem`** is the only emitter for `JumpEvent` and `LandEvent`.
It guards against missing queues with `try_resource`:
```cpp
auto* jump_ev = world.try_resource<Events<JumpEvent>>();
// ...
if (state.jump_impulse > 0.0f && jump_ev)
    jump_ev->send({e, state.jump_count, state.jump_impulse});
```

This means the demo runs correctly even without the event bus wired up — the
guard simply skips the send. This is important for test scenarios where
`Update` is not called directly, and for future systems that may conditionally
opt into events.

**Adding a new event consumer:**
```cpp
// In any system's Update:
if (auto* ev = world.try_resource<Events<JumpEvent>>()) {
    for (const auto& jump : ev->read()) {
        // react to jump.entity, jump.jump_number, jump.impulse
    }
}
```
No registration required on the consumer side. Just read.

**Adding a new event type:**
1. Add `struct MyEvent { ... };` to `events.hpp`.
2. Add `reg.register_queue<MyEvent>(world);` in `main.cpp` setup.
3. Emit in the appropriate system's `Update`.
4. Add a row to the event table in `ARCH_STATE.md`.

---

## 6. Trade-offs & Gotchas

**Events are one-frame only — no buffering across frames.**
If a consumer system runs before the emitter in the same frame, it will see an
empty queue. The ordering guarantee: `CharacterStateSystem` runs before
Physics and Render, so audio/effects systems in those phases can safely read
`JumpEvent` and `LandEvent`.

**`EventRegistry::flush_all()` must be the first Pre-Update step.**
If a system runs before the flush, it would read stale events from the
previous frame. The ordering in `main.cpp` enforces this — the flush lambda
is added before `InputGatherSystem`.

**`try_resource` guards are intentional, not defensive.**
They allow the engine to run without the event bus wired up (e.g., a minimal
test harness). Removing them and using `world.resource<>()` would panic on
missing resources in such contexts.

**`EventRegistry` stores lambdas that capture `world` by reference.**
The `world` variable in `main.cpp` outlives the registry and the entire game
loop. This is safe. Do not move or copy the `EventRegistry` after it has been
registered — the lambdas would still point to the original world.

**`Events<T>` is not thread-safe.**
All systems currently run single-threaded. If parallelism is introduced later,
event emission will need a mutex or a per-thread staging buffer.

---

## 7. Related Resources

* **RFC:** [`docs/rfcs/02-implemented/0009-event-system.md`](../rfcs/02-implemented/0009-event-system.md)
* **Header:** `src/events.hpp`
* **Emitter:** `src/systems/character_state.cpp` — `Update` function
* **Wiring:** `src/main.cpp` — EventRegistry setup + pre_update flush
* **Tests:** `tests/logic_tests.cpp` — `Events<T>` API test cases
* **ARCH_STATE.md:** Section 3 (Event Bus)
