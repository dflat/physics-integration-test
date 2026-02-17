# RFC-0004: Robust Linux Gamepad Discovery

* **Status:** Implemented
* **Date:** February 2026
* **Supersedes:** N/A
* **Superseded by:** N/A

## Summary

This RFC addresses a critical issue on Linux where many raylib-based games fail to detect gamepads due to a combination of low default slot limits (4) and overly broad device detection by the underlying GLFW library. It proposes increasing the slot limit to the GLFW maximum (16) and implementing a heuristic-based filtering layer to distinguish real controllers from "fake" joystick devices like trackpads and sensors.

## Motivation

On Linux, GLFW scans `/dev/input/event*` and registers any device with the `EV_ABS` capability as a joystick. This includes many non-gamepad devices:
- Keyboards with media controls
- Laptop trackpads
- Built-in accelerometers (IMUs)
- Audio jack sensors

Raylib 5.5 defaults to a `MAX_GAMEPADS` limit of 4. In modern Linux environments, it is common for the first 4-8 slots to be occupied by these "phantom" devices, causing actual gamepads (like a Nintendo Switch Pro Controller or DualSense) to be assigned to higher slots (e.g., 8 or 10) where they are never polled by the engine.

## Design

### Overview

The solution involves a two-pronged approach:
1. **Source Patching**: Modifying Raylib's internal configuration during the build process to expand its polling range.
2. **Filtering Heuristic**: Implementing an engine-level check to ignore non-gamepad devices while polling those expanded slots.

### API Changes

```cpp
// Added to GamepadInputSystem in src/systems/gamepad.hpp
class GamepadInputSystem {
public:
    static void Update(ecs::World& world);
    
    /**
     * @brief Determines if a hardware slot contains a real gamepad based on
     * capabilities and name heuristics.
     */
    static bool IsRealGamepad(int index);
};
```

### Implementation Details

#### Build-time Modification
We use a `sed` command in `CMakeLists.txt` within the `FetchContent_Declare(raylib ...)` block:
```cmake
PATCH_COMMAND sed -i "s/#define MAX_GAMEPADS.*/#define MAX_GAMEPADS 16/" src/config.h
```
This increases the pollable slots from 4 to 16, which is the maximum supported by GLFW's `GLFW_JOYSTICK_LAST`.

#### Heuristic Logic
The `IsRealGamepad` function in `src/systems/gamepad.cpp` uses three criteria:
1. **Availability**: `IsGamepadAvailable(index)` must be true.
2. **Axis Count**: Real gamepads must have at least 4 axes (mapping to LX, LY, RX, RY).
3. **Blacklist**: Device names must not contain keywords associated with non-gamepad devices (e.g., "SMC", "Accelerometer", "Trackpad", "Consumer Control").

#### Additive Input
To prevent keyboard and gamepad systems from fighting, the input gathering was changed from assignment to additive:
```cpp
// In GamepadInputSystem::Update
if (is_active) {
    if (std::abs(lx) > deadzone) input.move_input.x += lx;
    // ...
}
```

### Migration

This is a non-breaking change for developers. Existing code using `PlayerInput` will automatically benefit from wider controller support.

## Alternatives Considered

- **Dynamic Patching**: Using `.patch` files. Rejected because they are fragile to line-ending and whitespace changes in upstream Raylib.
- **Increasing to 32**: Attempted but rejected. GLFW triggers a core dump (assertion failure) if queried beyond slot 15 (`GLFW_JOYSTICK_16`).
- **Raw evdev/SDL2**: Implementing a custom input backend. Rejected as it would bypass Raylib's platform abstraction and increase maintenance burden.

## Testing

Verification was performed on an Arch Linux system with 10+ input devices:
1. **No-Regression**: Keyboard input remains functional.
2. **Detection**: Verified via `TraceLog` that "Pro Controller" at slot 8 and "DualSense" at slot 5 are correctly registered.
3. **Filtering**: Verified that "Apple Inc. Magic Trackpad" and "SMC" sensors were successfully ignored by the heuristic.
4. **Stability**: Confirmed no crashes on startup due to GLFW slot limits.

## Risks & Open Questions

- **Heuristic False Negatives**: Some very old controllers (e.g., SNES-style USB pads) may have fewer than 4 axes. These would be filtered out by the current heuristic. If support for these is required, the axis count check may need to be relaxed or made configurable.
- **Naming Variations**: Blacklist strings are case-sensitive and based on common Linux kernel naming conventions. Variations in drivers or OS kernels might require updating the blacklist.
