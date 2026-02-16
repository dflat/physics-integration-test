#include "gamepad.hpp"
#include "../components.hpp"
#include <raylib.h>
#include <cmath>
#include <algorithm>

using namespace ecs;

void GamepadInputSystem::Update(World& world) {
    // We scan standard slots. 16 is plenty for any modern OS.
    const int max_slots = 16;
    const float deadzone = 0.15f;

    world.single<PlayerInput>([&](Entity, PlayerInput& input) {
        for (int i = 0; i < max_slots; i++) {
            if (!IsGamepadAvailable(i)) continue;

            // Heuristic: A real modern gamepad should have at least 4 axes (2 sticks)
            // and at least 6 buttons. This filters out most "gaming mice" masquerading as gamepads.
            if (GetGamepadAxisCount(i) < 4) continue;

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
                if (std::abs(lx) > deadzone) input.move_input.x = lx;
                if (std::abs(ly) > deadzone) input.move_input.y = -ly; // Invert Y for world-space forward
                if (std::abs(rx) > deadzone) input.look_input.x = rx;
                if (std::abs(ry) > deadzone) input.look_input.y = ry;
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

        // 4. Final Input Normalization
        float move_mag_sq = input.move_input.x * input.move_input.x + input.move_input.y * input.move_input.y;
        if (move_mag_sq > 1.0f) {
            float mag = std::sqrt(move_mag_sq);
            input.move_input.x /= mag;
            input.move_input.y /= mag;
        }
    });
}
