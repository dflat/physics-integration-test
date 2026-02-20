#include "character_state.hpp"
#include "../components.hpp"
#include "../physics_handles.hpp"

using namespace ecs;

void CharacterStateSystem::Register(World& world) {
    world.on_add<CharacterControllerConfig>(
        [](World& w, Entity e, CharacterControllerConfig&) {
            w.add(e, CharacterState{});
        });
}

void CharacterStateSystem::Update(World& world, float dt) {
    world.each<CharacterHandle, CharacterIntent, CharacterState>(
        [dt](Entity, CharacterHandle& h, CharacterIntent& intent, CharacterState& state) {
            bool on_ground = h.character->GetGroundState() ==
                             JPH::CharacterVirtual::EGroundState::OnGround;
            apply_state(on_ground, dt, intent, state);
        });
}
