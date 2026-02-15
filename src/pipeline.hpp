#pragma once
#include <ecs/ecs.hpp>
#include <vector>
#include <functional>

namespace ecs {

/**
 * @brief Manages groups of systems categorized by execution phase.
 */
class Pipeline {
public:
    using SystemFunc = std::function<void(World&, float)>;

    void add_pre_update(SystemFunc func) { pre_update_.push_back(func); }
    void add_logic(SystemFunc func) { logic_.push_back(func); }
    void add_physics(SystemFunc func) { physics_.push_back(func); }
    void add_render(SystemFunc func) { render_.push_back(func); }

    /**
     * @brief Executes the standard update flow.
     */
    void update(World& world, float dt) {
        // 1. Input / Pre-processing
        for (auto& sys : pre_update_) sys(world, dt);
        
        // 2. Gameplay Logic
        for (auto& sys : logic_) sys(world, dt);

        // 3. Sync structural changes (e.g. spawned platforms) before physics
        world.deferred().flush(world);

        // 4. Simulation (Note: In fixed-step mode, this is called separately)
        // for (auto& sys : physics_) sys(world, dt);

        // 5. Cleanup / Sync structural changes before rendering
        world.deferred().flush(world);
    }

    /**
     * @brief Executes only the physics/simulation systems.
     */
    void step_physics(World& world, float dt) {
        for (auto& sys : physics_) sys(world, dt);
    }

    /**
     * @brief Executes rendering systems.
     */
    void render(World& world) {
        for (auto& sys : render_) sys(world, 0.0f);
    }

private:
    std::vector<SystemFunc> pre_update_;
    std::vector<SystemFunc> logic_;
    std::vector<SystemFunc> physics_;
    std::vector<SystemFunc> render_;
};

} // namespace ecs
