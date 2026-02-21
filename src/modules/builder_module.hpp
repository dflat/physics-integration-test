#pragma once
#include "../pipeline.hpp"
#include "../systems/builder.hpp"
#include <ecs/ecs.hpp>

// ---------------------------------------------------------------------------
// BuilderModule
//
// Adds PlatformBuilderSystem to the Logic phase. Runs after CharacterState
// (it reads PlayerInput and PlayerState) and before CharacterMotor.
// ---------------------------------------------------------------------------

struct BuilderModule {
    static void install(ecs::World& /*world*/, ecs::Pipeline& pipeline) {
        pipeline.add_logic([](ecs::World& w, float) { PlatformBuilderSystem::Update(w); });
    }
};
