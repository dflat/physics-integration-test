#include "components.hpp"
#include "events.hpp"
#include "scene.hpp"
#include "assets.hpp"
#include "audio_resource.hpp"
#include "debug_panel.hpp"
#include "physics_context.hpp"
#include "pipeline.hpp"
#include "systems/builder.hpp"
#include "systems/camera.hpp"
#include "systems/character_input.hpp"
#include "systems/character_state.hpp"
#include "systems/character_motor.hpp"
#include "systems/input_gather.hpp"
#include "systems/player_input.hpp"
#include "systems/physics.hpp"
#include "systems/audio.hpp"
#include "systems/debug.hpp"
#include "systems/renderer.hpp"
#include <ecs/ecs.hpp>
#include <ecs/modules/transform.hpp>
#include <ecs/modules/transform_propagation.hpp>
#include <raylib.h>

static const char* SCENE_PATH = "resources/scenes/default.json";

int main() {
  InitWindow(1280, 720, "Physics Integration - Dynamic Parkour");
  InitAudioDevice();
  SetTargetFPS(60);

  ecs::World world;
  
  AssetResource assets;
  assets.load();
  world.set_resource(assets);

  AudioResource audio;
  audio.load();
  world.set_resource(audio);

  // --- Debug Overlay Setup ---
  {
    DebugPanel panel;
    panel.watch("Engine", "FPS",
        []()       { return std::to_string(GetFPS()); });
    panel.watch("Engine", "Frame Time",
        []()       { char b[16]; std::snprintf(b, sizeof(b), "%d ms", (int)(GetFrameTime() * 1000)); return std::string(b); });
    panel.watch("Engine", "Entities",
        [&world]() { return std::to_string(world.count()); });
    panel.watch("Character", "Mode",
        [&world]() {
            std::string r = "-";
            world.each<CharacterState>([&](ecs::Entity, CharacterState& s) {
                r = (s.mode == CharacterState::Mode::Grounded) ? "Grounded" : "Airborne";
            });
            return r;
        });
    panel.watch("Character", "Jump Count",
        [&world]() {
            std::string r = "-";
            world.each<CharacterState>([&](ecs::Entity, CharacterState& s) { r = std::to_string(s.jump_count); });
            return r;
        });
    panel.watch("Character", "Air Time",
        [&world]() {
            std::string r = "-";
            world.each<CharacterState>([&](ecs::Entity, CharacterState& s) {
                char b[16]; std::snprintf(b, sizeof(b), "%.2f s", s.air_time);
                r = b;
            });
            return r;
        });
    panel.watch("Camera", "Mode",
        [&world]() {
            auto* cam = world.try_resource<MainCamera>();
            if (!cam) return std::string("-");
            return cam->follow_mode ? std::string("Follow") : std::string("Manual");
        });
    world.set_resource(std::move(panel));
  }

  PhysicsContext::InitJoltAllocator();
  world.set_resource(std::make_shared<PhysicsContext>());
  world.set_resource(MainCamera{});

  PhysicsSystem::Register(world);
  CharacterInputSystem::Register(world);
  CharacterStateSystem::Register(world);
  CharacterMotorSystem::Register(world);

  // --- Event Bus Setup ---
  world.set_resource(EventRegistry{});
  {
    auto& reg = world.resource<EventRegistry>();
    reg.register_queue<JumpEvent>(world);
    reg.register_queue<LandEvent>(world);
  }

  SceneLoader::load(world, SCENE_PATH);

  // --- Pipeline Configuration ---
  ecs::Pipeline pipeline;

  // 1. Input Phase — flush previous frame's events first, then gather input.
  pipeline.add_pre_update([](ecs::World& w, float) { w.resource<EventRegistry>().flush_all(); });
  pipeline.add_pre_update([](ecs::World& w, float) { InputGatherSystem::Update(w); });
  pipeline.add_pre_update([](ecs::World& w, float) { PlayerInputSystem::Update(w); });

  // 2. Logic Phase
  // Order matters: Camera writes view dirs → CharacterInput reads them → CharacterState
  // reads intent → CharacterMotor applies forces (must be last, before Physics).
  pipeline.add_logic([](ecs::World& w, float dt) { CameraSystem::Update(w, dt); });
  pipeline.add_logic([](ecs::World& w, float dt) { CharacterInputSystem::Update(w, dt); });
  pipeline.add_logic([](ecs::World& w, float dt) { CharacterStateSystem::Update(w, dt); });
  pipeline.add_logic([](ecs::World& w, float dt) { AudioSystem::Update(w, dt); });
  pipeline.add_logic([](ecs::World& w, float)    { PlatformBuilderSystem::Update(w); });
  pipeline.add_logic([](ecs::World& w, float dt) { CharacterMotorSystem::Update(w, dt); });

  // 3. Physics Phase (Fixed Step)
  pipeline.add_physics([](ecs::World& w, float dt) { 
      PhysicsSystem::Update(w, dt); 
      ecs::propagate_transforms(w);
  });

  // 4. Render Phase
  pipeline.add_render([](ecs::World& w, float)    { RenderSystem::Update(w); });
  pipeline.add_render([](ecs::World& w, float dt)  { DebugSystem::Update(w, dt); });

  // --- Main Loop ---
  float accumulator = 0.0f;
  const float fixed_dt = 1.0f / 60.0f;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    if (IsKeyPressed(KEY_R)) {
        SceneLoader::unload(world);
        SceneLoader::load(world, SCENE_PATH);
    }

    // 1. Update Logic & Input
    pipeline.update(world, dt);

    // 2. Step Physics (Fixed Timestep)
    accumulator += dt;
    while (accumulator >= fixed_dt) {
        pipeline.step_physics(world, fixed_dt);
        accumulator -= fixed_dt;
    }

    // 3. Render
    pipeline.render(world);
  }

  world.resource<AssetResource>().unload();
  world.resource<AudioResource>().unload();
  CloseAudioDevice();
  CloseWindow();
  return 0;
}
