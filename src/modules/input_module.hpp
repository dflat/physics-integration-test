#pragma once
#include "../pipeline.hpp"
#include "../systems/input_gather.hpp"
#include "../systems/player_input.hpp"
#include <ecs/ecs.hpp>

// ---------------------------------------------------------------------------
// InputModule
//
// Adds InputGatherSystem and PlayerInputSystem to the Pre-Update phase.
// InputGather must precede PlayerInput (it writes the InputRecord that
// PlayerInput reads). Both run after the EventBus flush.
// ---------------------------------------------------------------------------

struct InputModule {
    static void install(ecs::World& /*world*/, ecs::Pipeline& pipeline) {
        pipeline.add_pre_update([](ecs::World& w, float) { InputGatherSystem::Update(w); });
        pipeline.add_pre_update([](ecs::World& w, float) { PlayerInputSystem::Update(w); });
    }
};
