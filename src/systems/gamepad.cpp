#include "gamepad.hpp"
#include "../components.hpp"
#include <raylib.h>
#include <cmath>
#include <algorithm>
#include <string>

using namespace ecs;

bool GamepadInputSystem::IsRealGamepad(int i) {
    if (!IsGamepadAvailable(i)) return false;

    const char* name = GetGamepadName(i);
    int axis_count = GetGamepadAxisCount(i);
    
    // Heuristic 1: Minimum axes for a modern gamepad (LX, LY, RX, RY)
    // Most "fake" devices (buttons, sensors) have 0-2 axes.
    if (axis_count < 4) return false;

    // Heuristic 2: Button presence
    // Most fake joysticks on Linux don't map standard gamepad buttons.
    // We check for the South button (A/Cross) which is almost universal.
    // Note: We don't check if it's DOWN, just if the mapping exists.
    // In raylib, we can't easily check for mapping existence, so we rely on axes + blacklist.

    // Heuristic 3: Blacklist known non-gamepad devices that report as joysticks on Linux
    if (!name) return false;
    std::string n = name;
    
    // We must be careful not to match "Controller"
    const char* blacklist[] = {
        "Keyboard", "Mouse", "Trackpad", "Touchpad", 
        "SMC", "Accelerometer", "Mic", "Headset", 
        "Video", "Sensor", "Consumer Control", "System Control",
        "Power Button", "Speaker", "HDA Intel", "Apple Internal Keyboard"
    };

    for (const char* b : blacklist) {
        if (n.find(b) != std::string::npos) return false;
    }

    return true;
}

void GamepadInputSystem::Update(World& world) {
    // We scan up to 16 slots (matching GLFW's limit and our raylib patch).
    const int max_slots = 16;
    const float deadzone = 0.15f;
    
    static bool logged_info = false;
    int found_any_joystick = 0;
    int found_real_gamepads = 0;

    world.single<PlayerInput>([&](Entity, PlayerInput& input) {
        for (int i = 0; i < max_slots; i++) {
            bool available = IsGamepadAvailable(i);
            if (available) {
                found_any_joystick++;
                if (!logged_info) {
                    TraceLog(LOG_INFO, "INPUT: Slot %d available: '%s' (%d axes)", i, GetGamepadName(i), GetGamepadAxisCount(i));
                }
            }
            
            if (!IsRealGamepad(i)) continue;
            found_real_gamepads++;

            // 1. Sample Axes
            float lx = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_X);
            float ly = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_Y);
            float rx = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_X);
            float ry = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_Y);

            // 2. "Activity Wins" Logic
            // We only apply this slot's values if the user is actually touching it.
            // This prevents idle devices from zeroing out inputs from active ones.
            bool is_active = (std::abs(lx) > deadzone || std::abs(ly) > deadzone || 
                              std::abs(rx) > deadzone || std::abs(ry) > deadzone);

            if (is_active) {
                if (std::abs(lx) > deadzone) input.move_input.x += lx;
                if (std::abs(ly) > deadzone) input.move_input.y += -ly; // Invert Y for world-space forward
                if (std::abs(rx) > deadzone) input.look_input.x += rx;
                if (std::abs(ry) > deadzone) input.look_input.y += ry;
            }

            // 3. Sample Buttons (Additive/OR logic)
            // South button (Jump)
            if (IsGamepadButtonPressed(i, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                input.jump = true;
            }

            // West button (Camera Follow Toggle)
            if (IsGamepadButtonPressed(i, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) {
                // Note: We don't have direct access to MainCamera here, 
                // but the CameraSystem also polls gamepads for this toggle.
            }

            // Right Trigger (Plant Platform)
            float rt = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_TRIGGER);
            // Remap from [-1, 1] to [0, 1] for triggers that idle at -1.0
            float trigger_normalized = (rt + 1.0f) * 0.5f;
            if (trigger_normalized > 0.5f) {
                input.plant_platform = true;
                input.trigger_val = std::max(input.trigger_val, trigger_normalized);
            }
        }

        // Diagnostic: If we found joysticks but no gamepads, or if we are near the limit
        if (!logged_info) {
            if (found_any_joystick > 0 && found_real_gamepads == 0) {
                TraceLog(LOG_WARNING, "INPUT: Found %d joystick(s) but 0 passed Gamepad heuristic. Check /dev/input permissions or joycond.", found_any_joystick);
            } else if (found_real_gamepads > 0) {
                TraceLog(LOG_INFO, "INPUT: Successfully registered %d gamepad(s).", found_real_gamepads);
            }
            logged_info = true;
        }

        // 4. Final Input Normalization
        float move_mag_sq = input.move_input.x * input.move_input.x + input.move_input.y * input.move_input.y;
        if (move_mag_sq > 1.0f) {
            float mag = std::sqrt(move_mag_sq);
            input.move_input.x /= mag;
            input.move_input.y /= mag;
        }
    });
}
