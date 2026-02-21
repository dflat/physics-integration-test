#pragma once
#include "../debug_panel.hpp"
#include "../pipeline.hpp"
#include "../systems/debug.hpp"
#include <ecs/ecs.hpp>
#include <raylib.h>
#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// DebugModule
//
// Creates the DebugPanel world resource, registers Engine-level debug rows
// (FPS, Frame Time, Entity count), and adds DebugSystem to the Render phase
// (after RenderSystem).
//
// Must be installed BEFORE any game module that wants to add its own debug
// rows, so that the DebugPanel resource exists when those modules call
// world.try_resource<DebugPanel>()->watch(...).
// ---------------------------------------------------------------------------

struct DebugModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        DebugPanel panel;

        panel.watch("Engine", "FPS", []() {
            return std::to_string(GetFPS());
        });
        panel.watch("Engine", "Frame Time", []() {
            char b[16];
            std::snprintf(b, sizeof(b), "%d ms", (int)(GetFrameTime() * 1000));
            return std::string(b);
        });
        panel.watch("Engine", "Entities", [&world]() {
            return std::to_string(world.count());
        });

        world.set_resource(std::move(panel));
        pipeline.add_render([](ecs::World& w, float dt) { DebugSystem::Update(w, dt); });
    }
};
