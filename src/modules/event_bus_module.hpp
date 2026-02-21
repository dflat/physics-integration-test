#pragma once
#include "../events.hpp"
#include "../pipeline.hpp"
#include <ecs/ecs.hpp>

// ---------------------------------------------------------------------------
// EventBusModule
//
// Creates the EventRegistry world resource and installs the per-frame flush
// as the first Pre-Update step. Must be the first module installed so that
// all subsequent modules can call register_queue<T>() on a live registry.
// ---------------------------------------------------------------------------

struct EventBusModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        world.set_resource(EventRegistry{});
        pipeline.add_pre_update([](ecs::World& w, float) {
            w.resource<EventRegistry>().flush_all();
        });
    }
};
