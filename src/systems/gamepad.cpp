#include "gamepad.hpp"
#include "../components.hpp"
#include <raylib.h>
#include <cmath>
#include <algorithm>
#include <string>

using namespace ecs;

void GamepadInputSystem::Update(World& world) {
    // 1. Find all potential controllers (up to 8 slots)
    int active_gamepads[8];
    int active_count = 0;
    
    for (int i = 0; i < 8; i++) {
        if (IsGamepadAvailable(i)) {
            const char* name = GetGamepadName(i);
            if (name) {
                std::string n = name;
                // Heuristic: Ignore high-poll peripherals that masquerade as gamepads
                if (n.find("Glorious") != std::string::npos || 
                    n.find("Model D") != std::string::npos ||
                    n.find("Trackpad") != std::string::npos ||
                    n.find("EZ System Control") != std::string::npos) {
                    continue; 
                }
            }
            active_gamepads[active_count++] = i;
            
            static bool logged[8] = {false};
            if (!logged[i]) {
                TraceLog(LOG_INFO, "Gamepad %d ACCEPTED: %s", i, name);
                logged[i] = true;
            }
        }
    }

    if (active_count == 0) return;

    world.single<PlayerInput>([&](Entity, PlayerInput& input) {
        for (int j = 0; j < active_count; j++) {
            int i = active_gamepads[j];

            float lx = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_X);
            float ly = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_Y);
            float rx = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_X);
            float ry = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_Y);
            
            const float deadzone = 0.2f; // Slightly more aggressive deadzone
            
            // Only apply if there is actual movement. 
            // This prevents a "dead" gamepad in another slot from overwriting your active input with 0.
            if (std::abs(lx) > deadzone) input.move_input.x = lx;
            if (std::abs(ly) > deadzone) input.move_input.y = -ly;
            if (std::abs(rx) > deadzone) input.look_input.x = rx;
            if (std::abs(ry) > deadzone) input.look_input.y = ry;

            if (IsGamepadButtonPressed(i, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                input.jump = true;
            }

            // Right Trigger (Axis 4 or 5 usually)
            float rt = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_TRIGGER);
            // Remap from [-1, 1] to [0, 1] if it behaves like a standard trigger
            float trigger = (rt + 1.0f) * 0.5f;
            if (trigger > 0.5f) {
                input.plant_platform = true;
                input.trigger_val = std::max(input.trigger_val, trigger);
            }
        }

        float move_mag_sq = input.move_input.x * input.move_input.x + input.move_input.y * input.move_input.y;
        if (move_mag_sq > 1.0f) {
            float mag = std::sqrt(move_mag_sq);
            input.move_input.x /= mag;
            input.move_input.y /= mag;
        }
    });
}
