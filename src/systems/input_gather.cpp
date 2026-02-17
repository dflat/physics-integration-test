#include "input_gather.hpp"
#include "../input_state.hpp"
#include <raylib.h>
#include <string>

static bool IsRealGamepad(int i) {
    if (!IsGamepadAvailable(i)) return false;

    const char* name = GetGamepadName(i);
    int axis_count = GetGamepadAxisCount(i);
    if (axis_count < 4) return false;

    if (!name) return false;
    std::string n = name;
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

void InputGatherSystem::Update(ecs::World& world) {
    InputRecord* input_ptr = world.try_resource<InputRecord>();
    if (!input_ptr) {
        world.set_resource(InputRecord{});
        input_ptr = world.try_resource<InputRecord>();
    }
    auto& input = *input_ptr;

    // 1. Keyboard
    for (int i = 0; i < 512; i++) {
        input.keys_down[i] = IsKeyDown(i);
        input.keys_pressed[i] = IsKeyPressed(i);
    }

    // 2. Mouse
    input.mouse_pos = GetMousePosition();
    input.mouse_delta = GetMouseDelta();
    input.mouse_wheel = GetMouseWheelMove();
    for (int i = 0; i < 8; i++) {
        input.mouse_buttons[i] = IsMouseButtonDown(i);
        input.mouse_buttons_pressed[i] = IsMouseButtonPressed(i);
    }

    // 3. Gamepads
    input.gamepads.clear();
    for (int i = 0; i < 16; i++) {
        if (IsRealGamepad(i)) {
            GamepadState gp;
            gp.id = i;
            gp.connected = true;
            
            int axis_count = GetGamepadAxisCount(i);
            for (int a = 0; a < 8 && a < axis_count; a++) {
                gp.axes[a] = GetGamepadAxisMovement(i, a);
            }

            for (int b = 0; b < 32; b++) {
                gp.buttons[b] = IsGamepadButtonDown(i, b);
                gp.buttons_pressed[b] = IsGamepadButtonPressed(i, b);
            }
            input.gamepads.push_back(gp);
        }
    }
}
