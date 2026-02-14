#include "components.hpp"
#include "physics_context.hpp"
#include "systems/builder.hpp"
#include "systems/camera.hpp"
#include "systems/character.hpp"
#include "systems/gamepad.hpp"
#include "systems/physics.hpp"
#include "systems/renderer.hpp"
#include <ecs/ecs.hpp>
#include <ecs/modules/transform.hpp>
#include <ecs/modules/transform_propagation.hpp>
#include <raylib.h>
#include <raymath.h>

void System_Input(ecs::World &world) {
  world.single<PlayerInput>([&](ecs::Entity, PlayerInput &input) {
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

ecs::Quat ToEcs(Quaternion q) { return {q.x, q.y, q.z, q.w}; }

void SpawnScene(ecs::World &world) {
  // 1. Ground Plane
  auto ground = world.create();
  world.add(ground, ecs::LocalTransform{{0, -1, 0}, {0, 0, 0, 1}, {100, 1, 100}});
  world.add(ground, ecs::WorldTransform{});
  world.add(ground, MeshRenderer{0, DARKGRAY});
  world.add(ground, BoxCollider{{50, 0.5f, 50}});
  world.add(ground, RigidBodyConfig{BodyType::Static});

  // 2. Player
  auto player = world.create();
  world.add(player, ecs::LocalTransform{{0, 2, 0}});
  world.add(player, ecs::WorldTransform{});
  world.add(player, MeshRenderer{2, RED});
  world.add(player, PlayerTag{});
  world.add(player, PlayerInput{});
  world.add(player, PlayerState{});
  world.add(player, CharacterControllerConfig{});

  // 4. Camera
  auto camera = world.create();
  world.add(camera, MainCamera{});

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
  }
}

int main() {
  InitWindow(1280, 720, "Physics Integration - Dynamic Parkour");
  SetTargetFPS(60);

  ecs::World world;
  PhysicsContext::InitJoltAllocator();
  world.set_resource(std::make_shared<PhysicsContext>());

  PhysicsSystem::Register(world);
  CharacterSystem::Register(world);

  SpawnScene(world);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    if (IsKeyPressed(KEY_R)) {
        std::vector<ecs::Entity> to_destroy;
        world.each([&](ecs::Entity e) { to_destroy.push_back(e); });
        for (auto e : to_destroy) world.destroy(e);
        SpawnScene(world);
    }

    System_Input(world);
    GamepadInputSystem::Update(world);
    PlatformBuilderSystem::Update(world);
    world.deferred().flush(world); // Create platforms before physics step
    CameraSystem::Update(world, dt);
    CharacterSystem::Update(world, dt);
    PhysicsSystem::Update(world, dt);
    ecs::propagate_transforms(world);
    RenderSystem::Update(world);
    world.deferred().flush(world); // Catch-all for any other deferred commands
  }

  CloseWindow();
  return 0;
}
