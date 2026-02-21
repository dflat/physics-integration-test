#pragma once
#include "../physics_context.hpp"
#include "../pipeline.hpp"
#include "../systems/physics.hpp"
#include <ecs/ecs.hpp>
#include <ecs/modules/transform_propagation.hpp>
#include <memory>

// ---------------------------------------------------------------------------
// PhysicsModule
//
// Initialises Jolt's allocator, creates and registers the PhysicsContext
// world resource, installs PhysicsSystem lifecycle hooks (on_add/on_remove
// for RigidBodyConfig), and wires the fixed-step physics update +
// transform propagation into the Physics pipeline phase.
// ---------------------------------------------------------------------------

struct PhysicsModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        PhysicsContext::InitJoltAllocator();
        world.set_resource(std::make_shared<PhysicsContext>());
        PhysicsSystem::Register(world);
        pipeline.add_physics([](ecs::World& w, float dt) {
            PhysicsSystem::Update(w, dt);
            ecs::propagate_transforms(w);
        });
    }
};
