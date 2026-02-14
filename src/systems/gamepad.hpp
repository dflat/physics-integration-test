#pragma once
#include "../components.hpp"
#include <ecs/ecs.hpp>
#include <raylib.h>
#include <cmath>

using namespace ecs;

class GamepadInputSystem {
public:
    static void Update(World& world) {
        // Find the first available gamepad
        int gamepad = 0;
        bool found = false;
        for (int i = 0; i < 4; i++) {
            if (IsGamepadAvailable(i)) {
                gamepad = i;
                found = true;
                break;
            }
        }

        if (!found) return;

        world.each<PlayerInput>([&](Entity, PlayerInput& input) {
            // 1. Left Stick -> Movement (Y is inverted in Raylib axes vs our move logic)
            float lx = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
            float ly = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);
            
            // Deadzone and apply
            if (std::abs(lx) > 0.1f) input.move_input.x = lx;
            if (std::abs(ly) > 0.1f) input.move_input.y = -ly;

            // 2. Right Stick -> Look
            float rx = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X);
            float ry = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_Y);
            
            if (std::abs(rx) > 0.1f) input.look_input.x = rx;
            else input.look_input.x = 0;
            
            if (std::abs(ry) > 0.1f) input.look_input.y = ry;
            else input.look_input.y = 0;

            // 3. Buttons
            if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) input.jump = true;
        });
    }
};
