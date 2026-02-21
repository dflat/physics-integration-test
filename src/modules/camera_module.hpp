#pragma once
#include "../components.hpp"
#include "../debug_panel.hpp"
#include "../pipeline.hpp"
#include "../systems/camera.hpp"
#include <ecs/ecs.hpp>
#include <string>

// ---------------------------------------------------------------------------
// CameraModule
//
// Adds CameraSystem to the Logic phase and registers a "Camera" debug row.
//
// Pipeline placement: CameraSystem MUST be the first Logic-phase step — it
// writes view_forward / view_right to MainCamera, which CharacterInputSystem
// reads immediately after. Install this module before any other Logic-phase
// game module.
// ---------------------------------------------------------------------------

struct CameraModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        pipeline.add_logic([](ecs::World& w, float dt) { CameraSystem::Update(w, dt); });

        if (auto* panel = world.try_resource<DebugPanel>()) {
            panel->watch("Camera", "Mode", [&world]() {
                auto* cam = world.try_resource<MainCamera>();
                if (!cam) return std::string("-");
                return cam->follow_mode ? std::string("Follow") : std::string("Manual");
            });
        }
    }
};
