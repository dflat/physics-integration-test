# RFC-0000: Prototype Baseline

* **Status:** Baseline/Implemented
* **Date:** February 2026

## Summary

This RFC documents the baseline state of the game engine prototype as of
February 2026. It captures the current tech stack, architecture, and known
technical debt to serve as a reference point for future RFCs.

## Tech Stack

| Layer | Technology | Notes |
|-------|-----------|-------|
| ECS | Custom header-only (C++17) | Git submodule at `extern/ecs` |
| Physics | Jolt Physics v5.2.0 | FetchContent, SSE4.2 enabled |
| Rendering | Raylib (master) | FetchContent, custom GLSL shaders |
| Math | GLM | FetchContent |
| Testing | Catch2 v3.4.0 | FetchContent, math utility tests |
| Build | CMake 3.18+ | C++20 standard required |
| CI | GitHub Actions | Windows build + rolling release |

## Core Architecture

### ECS Integration
- `ecs::World` is the central data hub for all domains (physics, rendering,
  gameplay).
- Components are POD structs defined in `src/components.hpp`.
- Systems are stateless classes with static `Update()` and `Register()` methods.
- Physics body lifecycle managed via `on_add`/`on_remove` hooks on
  `RigidBodyConfig` and `RigidBodyHandle`.
- Deferred commands used for safe entity spawning during iteration
  (`PlatformBuilderSystem`).

### Execution Pipeline
Four-phase pipeline (`src/pipeline.hpp`) with strict ordering:

1. **Pre-Update** (variable dt): Input aggregation (keyboard + gamepad)
2. **Logic** (variable dt): Platform building, camera, character controller
3. **Physics** (fixed 60Hz): Jolt world step + `propagate_transforms()`
4. **Render** (variable dt): Raylib drawing, pure consumer of ECS state

### Component Inventory

| Component | Domain | Purpose |
|-----------|--------|---------|
| `LocalTransform` | ECS module | Position, rotation, scale |
| `WorldTransform` | ECS module | Computed world matrix |
| `RigidBodyConfig` | Physics | Authoring data for Jolt body creation |
| `RigidBodyHandle` | Physics | Runtime Jolt BodyID |
| `BoxCollider` | Physics | Box half-extents |
| `SphereCollider` | Physics | Sphere radius |
| `CharacterControllerConfig` | Physics | Character capsule settings |
| `CharacterHandle` | Physics | shared_ptr to Jolt CharacterVirtual |
| `MeshRenderer` | Rendering | Shape type (int), color, scale offset |
| `PlayerInput` | Gameplay | Unified input state (movement, look, actions) |
| `PlayerState` | Gameplay | Jump count, air time, cooldowns |
| `MainCamera` | Rendering | Orbit angles, lerp state, Camera3D output |
| `PlayerTag` | Gameplay | Marker for player entity |
| `WorldTag` | Gameplay | Marker for scene-resettable entities |

### System Inventory

| System | Phase | Role |
|--------|-------|------|
| `KeyboardInputSystem` | Pre-Update | KBM polling into PlayerInput |
| `GamepadInputSystem` | Pre-Update | Gamepad polling into PlayerInput |
| `PlatformBuilderSystem` | Logic | Deferred platform spawning |
| `CameraSystem` | Logic | Smart follow + manual orbit camera |
| `CharacterSystem` | Logic | Jolt character controller locomotion |
| `PhysicsSystem` | Physics | Jolt world step + transform sync |
| `RenderSystem` | Render | Raylib drawing with custom shaders |

## Known Technical Debt

### Coupling

1. **Camera writes back to PlayerInput**: `CameraSystem` computes
   `view_forward`/`view_right` and writes them into `PlayerInput`, creating a
   tight bidirectional dependency with `CharacterSystem`. Should use a separate
   `ViewDirection` component.

2. **Raylib/Jolt types in components**: `MainCamera` contains `Camera3D`
   (Raylib) and `JPH::Vec3` (Jolt). `MeshRenderer` uses `Color` (Raylib).
   This prevents headless testing and serialization.

3. **Camera system polls Raylib directly**: Mouse/keyboard input for camera
   control bypasses `PlayerInput`, creating a dual input path.

### Patterns

4. **Sequential `add()` on entity creation**: `SpawnScene()` creates entities
   via `create()` + N individual `add()` calls, causing N archetype migrations
   per entity. Should use `create_with<>()`.

5. **`shared_ptr` in `CharacterHandle`**: Inconsistent with `RigidBodyHandle`
   (which stores a plain ID). No `on_remove` cleanup hook for characters.

6. **Magic number shape types**: `MeshRenderer::shape_type` is a raw `int`
   (0=Box, 1=Sphere, 2=Capsule). Should be an enum.

7. **Silent default collider**: Physics system silently creates a 0.5 unit box
   if no collider component is present. Should assert or require explicit
   collider.

### Missing Infrastructure

8. **No event system**: Systems communicate only via shared components. No way
   to react to discrete events (jump, collision, damage).

9. **No batch destroy**: Scene reset requires two-pass collect-then-destroy
   pattern with heap allocation.

10. **No ECS-level tests**: `tests/logic_tests.cpp` only tests math utilities.
    No integration tests for ECS system behavior.
