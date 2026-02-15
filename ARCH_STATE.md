# Architectural State (ARCH_STATE.md)

This document describes the current architectural design, data flows, and technical stack of the Physics Integration Test project as of February 2026.

## 1. System Overview
The application is a 3D physics-driven prototype built using a data-oriented **Entity Component System (ECS)** architecture. It decouples simulation (Jolt), logic (Systems), and presentation (Raylib).

## 2. Component Definitions
Data is stored in POD (Plain Old Data) structures within `src/components.hpp`:

| Component | Responsibility |
| :--- | :--- |
| `LocalTransform` | Local PRS (Position, Rotation, Scale). |
| `WorldTransform` | Computed world matrix (Synced with Physics). |
| `RigidBodyConfig` | Authoring data for Jolt body creation. |
| `CharacterControllerConfig` | Settings for the Virtual Character (mass, slope limit). |
| `PlayerInput` | Unified input state (Movement, Look, Jump, Build). |
| `PlayerState` | Runtime gameplay state (Jump counts, build cooldowns). |
| `MainCamera` | Persistent camera state (Orbit angles, smoothing buffers). |
| `MeshRenderer` | Visual representation data (Shape type, Color). |
| `WorldTag` | Marker for entities that should be destroyed on scene reset. |

## 3. System Responsibilities
Systems are stateless logic blocks that operate on component queries:

- **`GamepadInputSystem`**: Aggregates input from all hardware slots into `PlayerInput`.
- **`PlatformBuilderSystem`**: Monitors `PlayerInput` and uses `CommandBuffer` to safely instantiate platform entities beneath the player.
- **`CameraSystem`**: Implements "Smart Follow" and manual orbit logic. It calculates the final `Camera3D` state and syncs view vectors back to `PlayerInput`.
- **`CharacterSystem`**: Manages the `JPH::CharacterVirtual` life-cycle. It applies locomotion logic, handles "Coyote Time" jumping, and performs `ExtendedUpdate` for ground adherence.
- **`PhysicsSystem`**: Wraps the Jolt world step and synchronizes `JPH::Body` transforms back into ECS `WorldTransform` components.
- **`RenderSystem`**: A pure consumer system that draws the world using Raylib and custom GLSL shaders based on the state in `MainCamera` and `MeshRenderer`.

## 4. Data Flow & Execution Order
Each frame in `main.cpp` follows a strict sequence to ensure data consistency:

1.  **Input Phase**: `System_Input` (KBM) -> `GamepadInputSystem`.
2.  **Creation Phase**: `PlatformBuilderSystem` -> `world.deferred().flush()`. (Platforms must exist before physics).
3.  **Logic Phase**: `CameraSystem` (Calculates view vectors) -> `CharacterSystem` (Uses view vectors for move direction).
4.  **Simulation Phase**: `PhysicsSystem::Update` (Step Jolt) -> `ecs::propagate_transforms`.
5.  **Presentation Phase**: `RenderSystem::Update` -> `world.deferred().flush()` (Cleanup).

## 5. Dependency Management
- **ECS**: Internal header-only library managed as a **Git Submodule** in `extern/ecs`.
- **Jolt Physics / Raylib / GLM**: Managed via **CMake FetchContent**, ensuring automated cross-platform dependency resolution.

## 6. Deployment & CI/CD
- **Cross-Platform Support**: Targeted for Linux (GCC/Clang) and Windows (MSVC).
- **CI**: GitHub Actions (`.github/workflows/windows-build.yml`) validates every push on `windows-latest`.
- **Artifacts**: Successful Windows builds produce a `windows-binary` artifact containing the executable and the `resources/` directory.
- **Resource Management**: A CMake `POST_BUILD` step ensures that shaders in `resources/` are deployed alongside the binary for consistent relative-path loading.
