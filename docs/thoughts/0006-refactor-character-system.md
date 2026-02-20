# RFC-0006: Character System Decoupling

* **Status:** Proposal
* **Date:** February 2026
* **Supersedes:** N/A
* **Superseded by:** N/A

## Summary

This RFC proposes splitting the monolithic `CharacterSystem` into three specialized systems: `CharacterInputSystem`, `CharacterStateSystem`, and `CharacterMotorSystem`. This separation of concerns will decouple raw input processing from physics execution, enabling cleaner testing, AI integration, and more robust state management.

## Motivation

The current `CharacterSystem::Update` function has become a "God System" that handles:
1.  **Input Translation:** Reading raw `InputState` and converting it to movement vectors.
2.  **State Logic:** determining if the character is grounded, jumping, or in the air.
3.  **Physics Execution:** directly applying velocities and rotations to the Jolt `CharacterVirtual` interface.
4.  **Transform Sync:** manually synchronizing Jolt positions back to ECS `LocalTransform`.

This coupling makes the system difficult to test (cannot test physics without mocking input), brittle to extend (adding AI requires hacking input), and prone to bugs where state changes (like jumping) are tightly coupled to specific frame updates.

## Design

### Overview

The refactor decomposes `CharacterSystem` into a pipeline of three systems, communicating via intermediate components:

1.  **`CharacterInputSystem`**:
    *   **Reads:** `InputState` (or AI commands).
    *   **Writes:** `CharacterIntent` (new component).
    *   **Responsibility:** Translates "press W" or "gamepad stick" into "move direction: (0,0,1)" and "jump: true".

2.  **`CharacterStateSystem`**:
    *   **Reads:** `CharacterIntent`, `CharacterHandle` (for ground checks).
    *   **Writes:** `CharacterState` (updated component).
    *   **Responsibility:** Manages the state machine (Grounded $\leftrightarrow$ Airborne, Jump Count). Determines *if* an action is allowed (e.g., can I jump?).

3.  **`CharacterMotorSystem`**:
    *   **Reads:** `CharacterIntent`, `CharacterState`.
    *   **Writes:** `CharacterHandle` (Jolt Physics).
    *   **Responsibility:** Applies the actual physical forces/velocities to the Jolt character controller based on the approved state and intent.

### API Changes

**New Component: `CharacterIntent`**
```cpp
struct CharacterIntent {
    glm::vec3 move_dir = {0, 0, 0}; // Normalized movement direction
    glm::vec3 look_dir = {0, 0, 0}; // Desired facing direction
    bool jump_requested = false;    // True if jump button was just pressed
    bool sprint_requested = false;
};
```

**Modified `CharacterState`**
(Existing component, but strictly defined ownership)
```cpp
struct CharacterState {
    enum class Mode { Grounded, Airborne, Swimming, Climbing };
    Mode current_mode = Mode::Grounded;
    
    int jump_count = 0;
    float air_time = 0.0f;
    // ... other state flags
};
```

### Implementation Details

1.  **`src/components.hpp`**: Add `CharacterIntent`.
2.  **`src/systems/character_input.cpp`**: Create new system. Move input reading logic here.
3.  **`src/systems/character_state.cpp`**: Create new system. Move ground check and state transition logic here.
4.  **`src/systems/character_motor.cpp`**: Create new system. Move velocity application and `ExtendedUpdate` logic here.
5.  **`src/pipeline.hpp`**: Update execution order:
    *   `InputSystem` (existing)
    *   `CharacterInputSystem`
    *   `CharacterStateSystem`
    *   `CharacterMotorSystem`
    *   `PhysicsSystem` (existing)

### Migration

Existing code in `CharacterSystem::Update` will be distributed. No external API changes for other systems, as they primarily interact with `CharacterHandle` or `LocalTransform`.

## Alternatives Considered

*   **Keep as-is:** Rejected because it hinders AI implementation and testing.
*   **Two Systems (Input & Physics):** Rejected because state logic (state machine) often needs to run independent of physics ticks or input polling (e.g., for animation state).

## Testing

*   **Unit Tests:** Create `CharacterStateSystem` tests that inject a mock `CharacterIntent` and verify state transitions (e.g., `jump_requested` + `Grounded` -> `Airborne`).
*   **Integration Tests:** Verify that a simulated key press results in a change in `LocalTransform` after full pipeline execution.

## Risks & Open Questions

*   **Performance:** Slight overhead from iterating entities three times instead of once. Considered negligible for the number of characters (likely < 100).
*   **Ordering:** Must ensure `CharacterMotor` runs *before* `PhysicsSystem::Update` to avoid one-frame lag.
