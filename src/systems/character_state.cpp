#include "character_state.hpp"
#include "../components.hpp"
#include "../physics_handles.hpp"
#include "../events.hpp"

using namespace ecs;

void CharacterStateSystem::Register(World& world) {
    world.on_add<CharacterControllerConfig>(
        [](World& w, Entity e, CharacterControllerConfig&) {
            w.add(e, CharacterState{});
        });
}

void CharacterStateSystem::Update(World& world, float dt) {
    auto* jump_ev = world.try_resource<Events<JumpEvent>>();
    auto* land_ev = world.try_resource<Events<LandEvent>>();

    world.each<CharacterHandle, CharacterIntent, CharacterState>(
        [&, dt](Entity e, CharacterHandle& h, CharacterIntent& intent, CharacterState& state) {
            CharacterState::Mode prev_mode = state.mode;

            bool on_ground = h.character->GetGroundState() ==
                             JPH::CharacterVirtual::EGroundState::OnGround;
            apply_state(on_ground, dt, intent, state);

            // jump_impulse > 0 is a one-frame signal set by apply_state
            if (state.jump_impulse > 0.0f && jump_ev)
                jump_ev->send({e, state.jump_count, state.jump_impulse});

            // Airborne → Grounded transition
            if (prev_mode == CharacterState::Mode::Airborne &&
                state.mode  == CharacterState::Mode::Grounded && land_ev)
                land_ev->send({e});
        });
}
