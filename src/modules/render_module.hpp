#pragma once
#include "../assets.hpp"
#include "../components.hpp"
#include "../pipeline.hpp"
#include "../systems/renderer.hpp"
#include <ecs/ecs.hpp>

// ---------------------------------------------------------------------------
// RenderModule
//
// Loads the AssetResource (shaders), creates the MainCamera world resource,
// and adds RenderSystem to the Render phase.
//
// shutdown() must be called before CloseWindow() to unload GPU resources.
// ---------------------------------------------------------------------------

struct RenderModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        AssetResource assets;
        assets.load();
        world.set_resource(assets);
        world.set_resource(MainCamera{});
        pipeline.add_render([](ecs::World& w, float) { RenderSystem::Update(w); });
    }

    static void shutdown(ecs::World& world) {
        world.resource<AssetResource>().unload();
    }
};
