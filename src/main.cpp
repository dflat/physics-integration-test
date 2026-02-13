#include "components.hpp"
#include "physics_context.hpp"
#include "systems/character.hpp"
#include "systems/physics.hpp"
#include "systems/renderer.hpp"
#include <ecs/ecs.hpp>
#include <ecs/modules/transform.hpp>
#include <ecs/modules/transform_propagation.hpp>
#include <raylib.h>

void System_Input(ecs::World &world) {
  world.single<PlayerInput>([&](ecs::Entity, PlayerInput &input) {
    input.move_input = {0, 0};
    input.jump = false;

    if (IsKeyDown(KEY_W)) input.move_input.y = 1.0f;
    if (IsKeyDown(KEY_S)) input.move_input.y = -1.0f;
    if (IsKeyDown(KEY_A)) input.move_input.x = -1.0f;
    if (IsKeyDown(KEY_D)) input.move_input.x = 1.0f;
    
    if (IsKeyPressed(KEY_SPACE)) input.jump = true;

    if (std::abs(input.move_input.x) > 0.1f || std::abs(input.move_input.y) > 0.1f) {
      float len = std::sqrt(input.move_input.x * input.move_input.x + input.move_input.y * input.move_input.y);
      input.move_input.x /= len;
      input.move_input.y /= len;
    }
  });
}

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
  world.add(player, CharacterControllerConfig{});

  // 3. Climbing Parkour
  struct Platform { ecs::Vec3 pos; ecs::Vec3 size; Color color; };
  std::vector<Platform> platforms = {
      {{5, 1, 5}, {4, 2, 4}, GRAY},
      {{10, 2.5f, 10}, {3, 1, 3}, DARKBLUE},
      {{15, 4.5f, 5}, {3, 1, 3}, MAROON},
      {{10, 7.0f, -5}, {4, 1, 4}, DARKGREEN},
      {{0, 9.0f, -10}, {5, 1, 5}, PURPLE},
      {{-10, 12.0f, -5}, {3, 1, 8}, GOLD},
      {{-5, 15.0f, 5}, {10, 1, 2}, SKYBLUE},
  };

  for (const auto& p : platforms) {
      auto ent = world.create();
      world.add(ent, ecs::LocalTransform{p.pos, {0,0,0,1}, p.size});
      world.add(ent, ecs::WorldTransform{});
      world.add(ent, MeshRenderer{0, p.color});
      world.add(ent, BoxCollider{{p.size.x*0.5f, p.size.y*0.5f, p.size.z*0.5f}});
      world.add(ent, RigidBodyConfig{BodyType::Static});
  }
}

int main() {
  InitWindow(1280, 720, "Physics Integration - Parkour");
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
        // Reset: Clear world manually by collecting IDs first
        std::vector<ecs::Entity> to_destroy;
        world.each([&](ecs::Entity e) {
            to_destroy.push_back(e);
        });
        for (auto e : to_destroy) world.destroy(e);
        SpawnScene(world);
    }

    System_Input(world);
    CharacterSystem::Update(world, dt);
    PhysicsSystem::Update(world, dt);
    ecs::propagate_transforms(world);
    RenderSystem::Update(world);
  }

  CloseWindow();
  return 0;
}
