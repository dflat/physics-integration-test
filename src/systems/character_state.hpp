#pragma once
#include <ecs/ecs.hpp>
#include "../components.hpp"

// Owns the character state machine: ground detection, coyote time, jump eligibility.
// Runs in the Logic phase, after CharacterInputSystem.
class CharacterStateSystem {
public:
    static void Register(ecs::World& world);
    static void Update(ecs::World& world, float dt);

    // Pure state transition — no Jolt dependency. Exposed for unit testing.
    // on_ground: result of CharacterVirtual::GetGroundState() == OnGround
    static void apply_state(bool on_ground, float dt,
                            const CharacterIntent& intent, CharacterState& state);
};
