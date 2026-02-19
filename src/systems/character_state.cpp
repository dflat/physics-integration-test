#include "character_state.hpp"
#include "../components.hpp"
#include <Jolt/Physics/Character/CharacterVirtual.h>

using namespace ecs;

void CharacterStateSystem::Register(World& world) {
    world.on_add<CharacterControllerConfig>(
        [](World& w, Entity e, CharacterControllerConfig&) {
            w.add(e, CharacterState{});
        });
}

void CharacterStateSystem::apply_state(bool on_ground, float dt,
                                       const CharacterIntent& intent,
                                       CharacterState& state) {
    state.jump_impulse = 0.0f;

    if (on_ground) {
        state.mode       = CharacterState::Mode::Grounded;
        state.jump_count = 0;
        state.air_time   = 0.0f;
    } else {
        state.mode      = CharacterState::Mode::Airborne;
        state.air_time += dt;
    }

    // Coyote window: first 0.2s of airborne time before any jump has been used
    bool can_coyote = (state.jump_count == 0 && state.air_time < 0.2f);
    bool can_jump   = on_ground || can_coyote || (state.jump_count < 2);

    if (intent.jump_requested && can_jump) {
        state.jump_impulse = (state.jump_count == 0) ? 12.0f : 10.0f;
        // Coyote jump: airborne but first jump — consume it before incrementing
        if (!on_ground && state.jump_count == 0) {
            state.jump_count = 1;
        }
        state.jump_count++;
    }
}

void CharacterStateSystem::Update(World& world, float dt) {
    world.each<CharacterHandle, CharacterIntent, CharacterState>(
        [dt](Entity, CharacterHandle& h, CharacterIntent& intent, CharacterState& state) {
            bool on_ground = h.character->GetGroundState() ==
                             JPH::CharacterVirtual::EGroundState::OnGround;
            apply_state(on_ground, dt, intent, state);
        });
}
