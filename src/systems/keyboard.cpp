#include "keyboard.hpp"
#include "../components.hpp"
#include <raylib.h>
#include <cmath>

using namespace ecs;

void KeyboardInputSystem::Update(World& world) {
    world.single<PlayerInput>([&](Entity, PlayerInput &input) {
        input.move_input = {0, 0};
        input.look_input = {0, 0};
        input.jump = false;
        input.plant_platform = false;
        input.trigger_val = 0.0f;

        if (IsKeyDown(KEY_W)) input.move_input.y = 1.0f;
        if (IsKeyDown(KEY_S)) input.move_input.y = -1.0f;
        if (IsKeyDown(KEY_A)) input.move_input.x = -1.0f;
        if (IsKeyDown(KEY_D)) input.move_input.x = 1.0f;
        
        if (IsKeyPressed(KEY_SPACE)) input.jump = true;
        
        if (IsKeyPressed(KEY_E) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            input.plant_platform = true;
            input.trigger_val = 1.0f;
        }

        if (std::abs(input.move_input.x) > 0.1f || std::abs(input.move_input.y) > 0.1f) {
            float len = std::sqrt(input.move_input.x * input.move_input.x + input.move_input.y * input.move_input.y);
            input.move_input.x /= len;
            input.move_input.y /= len;
        }
    });
}
