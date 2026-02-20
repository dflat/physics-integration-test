#include "character_input.hpp"
#include "../components.hpp"
#include "../physics_handles.hpp"

using namespace ecs;

void CharacterInputSystem::Register(World& world) {
    world.on_add<CharacterControllerConfig>(
        [](World& w, Entity e, CharacterControllerConfig&) {
            w.add(e, CharacterIntent{});
        });
}

void CharacterInputSystem::Update(World& world, float /*dt*/) {
    // View directions are owned by MainCamera and written by CameraSystem
    // each Logic tick before this system runs.
    auto* cam = world.try_resource<MainCamera>();
    if (!cam) return;

    const ecs::Vec3 view_fwd   = cam->view_forward;
    const ecs::Vec3 view_right = cam->view_right;

    world.each<PlayerTag, PlayerInput, CharacterIntent>(
        [&](Entity, PlayerTag&, PlayerInput& input, CharacterIntent& intent) {
            // Project the 2D move input onto the world-space xz-plane.
            JPH::Vec3 fwd   = MathBridge::ToJolt(view_fwd);
            JPH::Vec3 right = MathBridge::ToJolt(view_right);

            fwd.SetY(0);
            right.SetY(0);

            if (fwd.LengthSq() > 0.001f)
                fwd = fwd.Normalized();
            else
                fwd = JPH::Vec3::sAxisZ();

            if (right.LengthSq() > 0.001f)
                right = right.Normalized();
            else
                right = fwd.Cross(JPH::Vec3::sAxisY()).Normalized();

            JPH::Vec3 move = fwd * input.move_input.y + right * input.move_input.x;

            intent.move_dir         = MathBridge::FromJolt(move);
            intent.look_dir         = MathBridge::FromJolt(fwd);
            intent.jump_requested   = input.jump;
            intent.sprint_requested = false;
        });
}
