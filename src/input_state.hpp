#pragma once
#include <ecs/ecs.hpp>
#include <raylib.h>
#include <vector>

struct GamepadState {
    int id = -1;
    bool connected = false;
    float axes[8] = {0};
    bool buttons[32] = {false};
    bool buttons_pressed[32] = {false};
};

struct InputRecord {
    // Keyboard
    bool keys_down[512] = {false};
    bool keys_pressed[512] = {false};

    // Mouse
    Vector2 mouse_pos = {0, 0};
    Vector2 mouse_delta = {0, 0};
    float mouse_wheel = 0.0f;
    bool mouse_buttons[8] = {false};
    bool mouse_buttons_pressed[8] = {false};

    // Gamepads (Filtered/Real only)
    std::vector<GamepadState> gamepads;
};
