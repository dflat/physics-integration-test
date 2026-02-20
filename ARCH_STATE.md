# Architectural State (ARCH_STATE.md)

This document describes the current architectural design, data flows, and technical stack of the Physics Integration Test project as of February 2026.

## 1. System Overview
The application is a 3D physics-driven prototype built using a data-oriented **Entity Component System (ECS)** architecture. It decouples simulation (Jolt), logic (Systems), and presentation (Raylib).

## 2. Component Definitions
Components are split across two headers:
- **`src/components.hpp`** — pure ECS/standard-library types; no engine-library dependencies; safe to include in headless test targets.
- **`src/physics_handles.hpp`** — Jolt runtime handles and `MathBridge`; included only by systems that interact with the physics engine.

| Component | Responsibility |
| :--- | :--- |
| `LocalTransform` | Local PRS (Position, Rotation, Scale). |
| `WorldTransform` | Computed world matrix (Synced with Physics). |
| `RigidBodyConfig` | Authoring data for Jolt body creation. |
| `RigidBodyHandle` | Runtime Jolt BodyID, managed by PhysicsSystem lifecycle hooks. |
| `CharacterControllerConfig` | Authoring data for the Virtual Character (mass, slope limit). |
| `CharacterHandle` | Runtime Jolt CharacterVirtual, managed by CharacterMotorSystem. |
| `CharacterIntent` | World-space move/look intent for this frame. Written by CharacterInputSystem, read by CharacterStateSystem and CharacterMotorSystem. |
| `CharacterState` | Physics-side character state (mode, jump count, air time, jump impulse). Written by CharacterStateSystem, read by CharacterMotorSystem. |
| `PlayerInput` | Semantic hardware intent (move, look, jump, build). Written by PlayerInputSystem. |
| `PlayerState` | Builder-specific runtime state (cooldown, trigger edge). Owned by PlatformBuilderSystem. |
| `MainCamera` | Persistent camera state (orbit angles, smoothing buffers, view directions). |
| `MeshRenderer` | Visual representation data (shape type, color, scale offset). |
| `WorldTag` | Marker for entities destroyed on scene reset. |

## 3. Event Bus

Transient signals use a frame-scoped event bus (`src/events.hpp`). Each event
type has its own `Events<T>` resource in the World. `EventRegistry::flush_all()`
clears all queues as the first Pre-Update step each frame.

| Event | Emitter | Purpose |
| :--- | :--- | :--- |
| `JumpEvent` | `CharacterStateSystem` | Fired once when `jump_impulse > 0`; carries `jump_number` (1 or 2) and `impulse` (m/s). |
| `LandEvent` | `CharacterStateSystem` | Fired once on Airborne → Grounded transition. |

## 4. System Responsibilities
Systems are stateless logic blocks that operate on component queries:

| System | Phase | Reads | Writes |
| :--- | :--- | :--- | :--- |
| `InputGatherSystem` | Pre-Update | Raylib hardware | `InputRecord` resource |
| `PlayerInputSystem` | Pre-Update | `InputRecord` | `PlayerInput` |
| `CameraSystem` | Logic | `InputRecord`, `PlayerInput`, `CharacterHandle`, `WorldTransform` | `MainCamera` (including view dirs) |
| `CharacterInputSystem` | Logic | `MainCamera` (view dirs), `PlayerInput` (move/jump) | `CharacterIntent` |
| `CharacterStateSystem` | Logic | `CharacterHandle` (ground query), `CharacterIntent` | `CharacterState` |
| `PlatformBuilderSystem` | Logic | `PlayerInput`, `PlayerState`, `WorldTransform` | Deferred entity creation |
| `CharacterMotorSystem` | Logic | `CharacterIntent`, `CharacterState`, `CharacterHandle` | Jolt velocities; `LocalTransform`, `WorldTransform` |
| `PhysicsSystem` | Physics (60 Hz) | `RigidBodyConfig`, `LocalTransform` | `RigidBodyHandle`; syncs `WorldTransform` from Jolt |
| `RenderSystem` | Render | `WorldTransform`, `MeshRenderer`, `MainCamera`, `AssetResource` | — (pure consumer) |

## 4. Data Flow & Execution Order
Each frame follows a strict four-phase sequence:

```
Pre-Update:  InputGather → PlayerInput
Logic:       Camera → CharacterInput → CharacterState → PlatformBuilder → CharacterMotor
             └─ deferred().flush() (spawned platforms materialise before physics)
Physics:     PhysicsSystem (60 Hz fixed step) → propagate_transforms
Render:      RenderSystem
             └─ deferred().flush() (cleanup)
```

The Logic ordering is a hard constraint:
- `Camera` must precede `CharacterInput` — it writes `view_forward`/`view_right` to the `MainCamera` resource, which `CharacterInputSystem` reads to project 2D move input into world space.
- `CharacterState` must precede `CharacterMotor` — the motor reads `jump_impulse` set by the state machine.
- `CharacterMotor` must be the last Logic system — it calls `ExtendedUpdate` on the Jolt character, which must run before the physics step.

## 5. Dependency Management
- **ECS**: Internal header-only library managed as a **Git Submodule** in `extern/ecs`.
- **Jolt Physics / Raylib / GLM**: Managed via **CMake FetchContent**, ensuring automated cross-platform dependency resolution.

## 6. Deployment & CI/CD
- **Cross-Platform Support**: Targeted for Linux (GCC/Clang) and Windows (MSVC).
- **CI**: GitHub Actions (`.github/workflows/windows-build.yml`) validates every push on `windows-latest`.
- **Artifacts**: Successful Windows builds produce a `windows-binary` artifact containing the executable and the `resources/` directory.
- **Resource Management**: A CMake `POST_BUILD` step ensures that shaders in `resources/` are deployed alongside the binary for consistent relative-path loading.
