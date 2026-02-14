#pragma once
#include "../components.hpp"
#include <ecs/ecs.hpp>
#include <raylib.h>
#include <cmath>
#include <algorithm>

using namespace ecs;

class GamepadInputSystem {
public:
    static void Update(World& world) {
        // 1. Determine which gamepads are actually connected this frame
        static int active_gamepads[4];
        int active_count = 0;
        for (int i = 0; i < 4; i++) {
            if (IsGamepadAvailable(i)) {
                active_gamepads[active_count++] = i;
            }
        }

        // 2. Early exit if no gamepads are connected
        if (active_count == 0) return;

        world.each<PlayerTag, PlayerInput>([&](Entity, PlayerTag&, PlayerInput& input) {
            for (int j = 0; j < active_count; j++) {
                int i = active_gamepads[j];

                // 1. Left Stick -> Movement
                float lx = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_X);
                float ly = GetGamepadAxisMovement(i, GAMEPAD_AXIS_LEFT_Y);
                
                // We use a slightly larger deadzone for safety
                const float deadzone = 0.15f;
                
                if (std::abs(lx) > deadzone) {
                    input.move_input.x = lx;
                }
                if (std::abs(ly) > deadzone) {
                    // ly is positive down in Raylib, so we negate it for our "forward" (positive Y)
                    input.move_input.y = -ly;
                }

                // 2. Right Stick -> Look (Additive to mouse)
                float rx = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_X);
                float ry = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_Y);
                
                if (std::abs(rx) > deadzone) {
                    input.look_input.x = rx;
                }
                if (std::abs(ry) > deadzone) {
                    input.look_input.y = ry;
                }

                // 3. Buttons
                // South button (Jump)
                if (IsGamepadButtonPressed(i, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                    input.jump = true;
                }

                // Right Trigger (Plant Platform)
                float rt = GetGamepadAxisMovement(i, GAMEPAD_AXIS_RIGHT_TRIGGER);
                input.trigger_val = std::max(input.trigger_val, rt);
                if (rt > 0.5f) {
                    input.plant_platform = true;
                }
            }

            // Optional: Normalize move_input if it exceeds 1.0 (combined keyboard + gamepad)
            float move_mag_sq = input.move_input.x * input.move_input.x + input.move_input.y * input.move_input.y;
            if (move_mag_sq > 1.0f) {
                float mag = std::sqrt(move_mag_sq);
                input.move_input.x /= mag;
                input.move_input.y /= mag;
            }
        });
    }
};
