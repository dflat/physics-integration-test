#pragma once
#include <ecs/ecs.hpp>

// ---------------------------------------------------------------------------
// AudioSystem — Logic-phase system; consumes JumpEvent and LandEvent.
//
// No Register() — no lifecycle hooks. AudioResource is loaded explicitly in
// main.cpp (same pattern as AssetResource).
// ---------------------------------------------------------------------------

class AudioSystem {
public:
    static void Update(ecs::World& world, float dt);
};
