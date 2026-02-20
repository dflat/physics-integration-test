#pragma once
#include <ecs/ecs.hpp>

// ---------------------------------------------------------------------------
// DebugSystem — Render-phase system; drives the debug overlay.
//
// No Register() — no lifecycle hooks.
// Toggle visibility with F3.
// ---------------------------------------------------------------------------

class DebugSystem {
public:
    static void Update(ecs::World& world, float dt);
};
