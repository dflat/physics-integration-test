# RFC-0005: Unified Input State

* **Status:** Implemented
* **Date:** February 2026
* **Supersedes:** RFC-0004 (Partial implementation details)

## Summary

This RFC proposes and implements a unified input architecture where all hardware state (Keyboard, Mouse, Gamepad) is queried exactly once per frame and stored in a solidified `InputRecord` resource. All downstream systems (Player Control, Camera, UI) consume this record rather than polling hardware directly, ensuring frame-wide consistency and reducing redundant processing.

## Motivation

Previously, input was scattered across multiple systems:
- `KeyboardInputSystem` polled keys directly.
- `GamepadInputSystem` polled controllers and ran expensive name-based filtering heuristics for every player.
- `CameraSystem` performed its own hardware polling for zoom and orbit controls.

This led to several issues:
1. **Performance**: Redundant string matching and blacklist checks for gamepad filtering every frame.
2. **Inconsistency**: Hardware state could theoretically change between the execution of the Movement system and the Camera system, leading to "glitchy" behavior in a single frame.
3. **Complexity**: Any new system requiring input had to re-implement or call into complex raylib/heuristic logic.

## Design

### Overview

We introduce a "Gather-Process" pattern:
1. **Gather**: A single system (`InputGatherSystem`) queries Raylib for all raw state and populates a global `InputRecord`.
2. **Process**: High-level systems (`PlayerInputSystem`, `CameraSystem`) read the `InputRecord` and translate raw state into semantic actions (e.g., "Jump", "Orbit").

### API Changes

```cpp
// src/input_state.hpp
struct GamepadState {
    int id;
    float axes[8];
    bool buttons[32];
    bool buttons_pressed[32];
};

struct InputRecord {
    bool keys_down[512];
    bool keys_pressed[512];
    Vector2 mouse_pos;
    Vector2 mouse_delta;
    std::vector<GamepadState> gamepads;
};

// Unified Systems
class InputGatherSystem { static void Update(ecs::World& world); };
class PlayerInputSystem { static void Update(ecs::World& world); };
```

### Implementation Details

1. **`InputRecord` Resource**: A global ECS resource holding the frame snapshot.
2. **Heuristic Consolidation**: The `IsRealGamepad` logic from RFC-0004 is moved into `InputGatherSystem`, running once per slot instead of once per consumer.
3. **Pipeline Order**:
   - `InputGatherSystem`: Queries Raylib (Must be first).
   - `PlayerInputSystem`: Maps record to `PlayerInput` components.
   - `CameraSystem`: Maps record to orbit/zoom logic.

### Migration

- **Removed**: `KeyboardInputSystem` and `GamepadInputSystem`.
- **Action**: Developers must add `InputGatherSystem` to the start of their pipeline. All input logic should now reference `world.resource<InputRecord>()` or `try_resource<InputRecord>()`.

## Alternatives Considered

- **Event-Based Input**: Pushing input events into a queue. Rejected for this prototype as polling a solidified record is simpler to reason about for physics-heavy movement logic.
- **Direct Raylib Polling**: Rejected due to the performance and consistency issues noted in Motivation.

## Testing

- **Functional**: Verified that WASD, Mouse Orbit, and Gamepad controls (including the Pro Controller) work exactly as before.
- **Co-op**: Verified that `InputRecord` correctly stores state for multiple connected gamepads.
- **Initialization**: Verified that `InputGatherSystem` safely self-initializes the `InputRecord` resource if it's missing.

## Risks & Open Questions

- **Memory**: The `InputRecord` stores 512 bools for keys. This is negligible (~0.5 KB) but could be optimized to a bitset if needed for extremely constrained environments.
