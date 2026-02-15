#include "components.hpp"
#include "physics_context.hpp"
#include "pipeline.hpp"
#include "systems/builder.hpp"
#include "systems/camera.hpp"
#include "systems/character.hpp"
#include "systems/gamepad.hpp"
#include "systems/keyboard.hpp"
#include "systems/physics.hpp"
#include "systems/renderer.hpp"
#include <ecs/ecs.hpp>
#include <ecs/modules/transform.hpp>
#include <ecs/modules/transform_propagation.hpp>
#include <raylib.h>
#include <raymath.h>

ecs::Quat ToEcs(Quaternion q) { return {q.x, q.y, q.z, q.w}; }


void SpawnScene(ecs::World &world) {
  // 1. Ground Plane
  auto ground = world.create();
  world.add(ground, ecs::LocalTransform{{0, -1, 0}, {0, 0, 0, 1}, {100, 1, 100}});
  world.add(ground, ecs::WorldTransform{});
  world.add(ground, MeshRenderer{0, DARKGRAY});
  world.add(ground, BoxCollider{{50, 0.5f, 50}});
  world.add(ground, RigidBodyConfig{BodyType::Static});
  world.add(ground, WorldTag{});

  // 2. Player
  auto player = world.create();
  world.add(player, ecs::LocalTransform{{0, 2, 0}});
  world.add(player, ecs::WorldTransform{});
  world.add(player, MeshRenderer{2, RED});
  world.add(player, PlayerTag{});
  world.add(player, PlayerInput{});
  world.add(player, PlayerState{});
  world.add(player, CharacterControllerConfig{});
  world.add(player, WorldTag{});

  // 3. Climbing Parkour with Inclined Planes
  struct Platform { 
    ecs::Vec3 pos; 
    ecs::Quat rot;
    ecs::Vec3 size; 
    Color color; 
  };

  ecs::Quat q_id = {0,0,0,1};
  ecs::Quat q_pitch_20 = ToEcs(QuaternionFromAxisAngle({1, 0, 0}, 20.0f * DEG2RAD));
  ecs::Quat q_pitch_40 = ToEcs(QuaternionFromAxisAngle({1, 0, 0}, 40.0f * DEG2RAD));
  ecs::Quat q_roll_20 = ToEcs(QuaternionFromAxisAngle({0, 0, 1}, 20.0f * DEG2RAD));

  std::vector<Platform> platforms = {
      {{5, 1, 5}, q_id, {4, 2, 4}, GRAY},
      {{10, 2.5f, 10}, q_id, {3, 1, 3}, DARKBLUE},
      // Ramps
      {{15, 2.0f, 0}, q_pitch_20, {8, 1, 4}, DARKGREEN},
      {{20, 5.0f, -5}, q_pitch_40, {8, 1, 4}, MAROON},
      {{15, 8.0f, -15}, q_roll_20, {4, 1, 8}, PURPLE},
      
      // Higher platforms
      {{0, 10.0f, -15}, q_id, {6, 1, 6}, GOLD},
      {{-10, 13.0f, -10}, q_id, {4, 1, 4}, SKYBLUE},
      {{-15, 16.0f, 0}, q_pitch_20, {5, 1, 10}, ORANGE},
      
      // Floating steps
      {{-5, 18.0f, 10}, q_id, {2, 0.5f, 2}, LIME},
      {{0, 20.0f, 15}, q_id, {2, 0.5f, 2}, LIME},
      {{5, 22.0f, 20}, q_id, {2, 0.5f, 2}, LIME},
  };

  for (const auto& p : platforms) {
      auto ent = world.create();
      world.add(ent, ecs::LocalTransform{p.pos, p.rot, p.size});
      world.add(ent, ecs::WorldTransform{});
      world.add(ent, MeshRenderer{0, p.color});
      world.add(ent, BoxCollider{{p.size.x*0.5f, p.size.y*0.5f, p.size.z*0.5f}});
      world.add(ent, RigidBodyConfig{BodyType::Static});
      world.add(ent, WorldTag{});
  }
}

int main() {
  InitWindow(1280, 720, "Physics Integration - Dynamic Parkour");
  SetTargetFPS(60);

  ecs::World world;
  PhysicsContext::InitJoltAllocator();
  world.set_resource(std::make_shared<PhysicsContext>());
  world.set_resource(MainCamera{});

  PhysicsSystem::Register(world);
  CharacterSystem::Register(world);

  SpawnScene(world);

  // --- Pipeline Configuration ---
  ecs::Pipeline pipeline;
  
  // 1. Input Phase
  pipeline.add_pre_update([](ecs::World& w, float) { KeyboardInputSystem::Update(w); });
  pipeline.add_pre_update([](ecs::World& w, float) { GamepadInputSystem::Update(w); });

  // 2. Logic Phase
  pipeline.add_logic([](ecs::World& w, float) { PlatformBuilderSystem::Update(w); });
  pipeline.add_logic([](ecs::World& w, float dt) { CameraSystem::Update(w, dt); });
  pipeline.add_logic([](ecs::World& w, float dt) { CharacterSystem::Update(w, dt); });

  // 3. Physics Phase (Fixed Step)
  pipeline.add_physics([](ecs::World& w, float dt) { 
      PhysicsSystem::Update(w, dt); 
      ecs::propagate_transforms(w);
  });

  // 4. Render Phase
  pipeline.add_render([](ecs::World& w, float) { RenderSystem::Update(w); });

  // --- Main Loop ---
  float accumulator = 0.0f;
  const float fixed_dt = 1.0f / 60.0f;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    if (IsKeyPressed(KEY_R)) {
        std::vector<ecs::Entity> to_destroy;
        world.each<WorldTag>([&](ecs::Entity e, WorldTag&) { to_destroy.push_back(e); });
        for (auto e : to_destroy) world.destroy(e);
        world.deferred().flush(world); 
        SpawnScene(world);
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

  CloseWindow();
  return 0;
}
