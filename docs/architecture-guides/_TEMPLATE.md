# ARCH-NNNN: [System / Subsystem Name]

* **RFC Reference:** [RFC-NNNN](../rfcs/02-implemented/NNNN-name.md)
* **Implementation Date:** YYYY-MM-DD
* **Status:** Active | Deprecated | Replaced by ARCH-XXXX

---

## 1. High-Level Mental Model

> 2–3 sentences: what does this subsystem do, and what is the core pattern or
> metaphor it follows? A reader should be able to explain it to someone else after
> reading only this block.

* **Core Responsibility:** The single most important job of this code.
* **Pipeline Phase:** Which phase(s) this runs in and why that ordering is required.
* **Key Constraints:** Performance budget, thread-safety requirements, ordering
  invariants that must never be violated.

---

## 2. The Grand Tour

*Do not list every file. List the two or three "anchor files" a developer should
open first, and say exactly what role each one plays.*

| File | Role |
|------|------|
| `src/systems/foo.hpp` | Public interface — the API other systems see. Start here. |
| `src/systems/foo.cpp` | Implementation — where the frame logic and lifecycle hooks live. |
| `src/components.hpp` | The component(s) this system owns. Look for `FooComponent` and `FooState`. |

### Lifecycle vs. Per-Frame Work

Most systems have two distinct entry points. Understand both before reading the
implementation:

**`Register(World&)`** — called once at startup, before `SpawnScene`.
- What `on_add` / `on_remove` hooks are registered and why.
- What heavyweight resources (Jolt bodies, GPU handles, etc.) are created here.
- What components are auto-provisioned when a trigger component is added.

**`Update(World&, float dt)`** — called every frame in the appropriate phase.
- What the per-frame query looks like (`world.each<...>`).
- What it reads, what it writes, in what order.
- Whether it issues deferred commands and when those are flushed.

---

## 3. Components & Data Ownership

*In an ECS, ownership is expressed through which system reads and writes which
components. Be explicit here.*

| Component | Owner (writer) | Consumers (readers) | Notes |
|-----------|---------------|---------------------|-------|
| `FooConfig` | Scene setup / `main.cpp` | `FooSystem::Register` hook | Authoring data; triggers resource creation |
| `FooHandle` | `FooSystem::Register` (on_add hook) | `FooSystem::Update` | Runtime handle; created from Config |
| `FooState` | `FooSystem::Update` | Downstream systems | Mutable per-frame output |

**Deferred Commands:** If this system creates or destroys entities mid-frame, note
it here. Deferred commands are flushed by the pipeline between Logic and Physics.

**Resources:** If this system reads or writes any `world.resource<T>()` singletons,
list them and describe the access pattern (read-only vs. read-write).

---

## 4. Data Flow

*Trace a single "unit of work" from input to output — one entity, one event, one
frame tick.*

```
Input:          [describe what triggers this system — a component being present,
                 a resource being set, a lifecycle hook firing]
        │
        ▼
Transform:      [describe the key computation or state change]
        │
        ▼
Output:         [what component or resource holds the result]
        │
        ▼
Consumed by:    [which downstream system reads this output]
```

---

## 5. System Integration (The Social Map)

*How does this system fit into the execution pipeline alongside its neighbours?*

**Upstream — must run before this system:**
- `SomePrecedingSystem` — because it writes `ComponentX`, which this system reads.

**Downstream — must run after this system:**
- `SomeFollowingSystem` — because it reads `ComponentY`, which this system writes.

**Ordering constraint summary:**
```
... → PrecedingSystem → [ThisSystem] → FollowingSystem → ...
```
If the ordering constraint is a hard invariant (e.g., "motor must run before the
physics step"), call that out explicitly and explain why a one-frame lag would occur
otherwise.

---

## 6. Trade-offs & Gotchas

*Document the "lessons learned" between RFC proposal and shipped code. This is the
section future maintainers will search first when something breaks.*

**Deviations from the RFC:**
- "During implementation we found X, so we changed Y to Z."

**Fragile areas:**
- Anything that is sensitive to execution order, floating-point precision, or
  implicit state. If a future change could silently break this, say so.

**Known limitations:**
- Hard-coded constants, maximum counts, or other bounds that would need revisiting
  if the scope changes (e.g., "capped at 100 character entities; beyond that the
  each<> query overhead becomes measurable").

---

## 7. Related Resources

* **RFC:** [`docs/rfcs/02-implemented/NNNN-name.md`](../rfcs/02-implemented/NNNN-name.md)
* **Tests:** `tests/logic_tests.cpp` — section `[subsystem-tag]`
* **Components:** `src/components.hpp` — search for `FooConfig`, `FooHandle`, `FooState`
* **ARCH_STATE.md:** Section 3 (System Responsibilities table) for a one-line summary
