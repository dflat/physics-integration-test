#include "modules/event_bus_module.hpp"
#include "modules/input_module.hpp"
#include "modules/physics_module.hpp"
#include "modules/render_module.hpp"
#include "modules/audio_module.hpp"
#include "modules/debug_module.hpp"
#include "modules/camera_module.hpp"
#include "modules/character_module.hpp"
#include "modules/builder_module.hpp"
#include "scene.hpp"
#include <ecs/ecs.hpp>
#include <raylib.h>

static const char* SCENE_PATH = "resources/scenes/default.json";

int main() {
    InitWindow(1280, 720, "Physics Integration - Dynamic Parkour");
    SetTargetFPS(60);

    ecs::World    world;
    ecs::Pipeline pipeline;

    // --- Engine Modules ---
    // These add only to Pre-Update, Physics, and Render phases — not Logic.
    // Installation order within this group is flexible.
    EventBusModule::install(world, pipeline);  // Pre-Update: event flush (must be first)
    InputModule::install(world, pipeline);     // Pre-Update: input gather + player input
    PhysicsModule::install(world, pipeline);   // Physics:    Jolt step + propagate_transforms
    RenderModule::install(world, pipeline);    // Render:     3D scene
    DebugModule::install(world, pipeline);     // Render:     debug overlay (before game modules)

    // --- Game Modules ---
    // Logic ordering is a hard constraint (see ARCH-0013).
    // Pipeline::add_logic appends in call order, so install order = execution order.
    //
    //   Camera → CharInput → CharState → Audio → Builder → CharMotor
    //
    CameraModule::install(world, pipeline);             // Logic[1]: Camera (must be first)
    CharacterModule::install(world, pipeline);          // Logic[2,3]: CharInput, CharState
    AudioModule::install(world, pipeline);              // Logic[4]: Audio + device/resource setup
    BuilderModule::install(world, pipeline);            // Logic[5]: PlatformBuilder
    CharacterModule::install_motor(world, pipeline);    // Logic[6]: CharMotor (must be last)

    // --- Scene ---
    SceneLoader::load(world, SCENE_PATH);

    // --- Game Loop ---
    float accumulator  = 0.0f;
    const float fixed_dt = 1.0f / 60.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_R)) {
            SceneLoader::unload(world);
            SceneLoader::load(world, SCENE_PATH);
        }

        pipeline.update(world, dt);

        accumulator += dt;
        while (accumulator >= fixed_dt) {
            pipeline.step_physics(world, fixed_dt);
            accumulator -= fixed_dt;
        }

        pipeline.render(world);
    }

    // --- Shutdown ---
    AudioModule::shutdown(world);   // unload sounds + CloseAudioDevice
    RenderModule::shutdown(world);  // unload shaders
    CloseWindow();
    return 0;
}
