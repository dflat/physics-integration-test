#include "character_input.hpp"
#include "../components.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>

using namespace ecs;

void CharacterInputSystem::Register(World& world) {
    world.on_add<CharacterControllerConfig>(
        [](World& w, Entity e, CharacterControllerConfig&) {
            w.add(e, CharacterIntent{});
        });
}

void CharacterInputSystem::Update(World& world, float /*dt*/) {
    world.each<PlayerTag, PlayerInput, CharacterIntent>(
        [](Entity, PlayerTag&, PlayerInput& input, CharacterIntent& intent) {
            // Project the 2D move input onto the world-space xz-plane using the
            // view directions written by CameraSystem into PlayerInput.
            JPH::Vec3 fwd   = MathBridge::ToJolt(input.view_forward);
            JPH::Vec3 right = MathBridge::ToJolt(input.view_right);

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

            intent.move_dir       = MathBridge::FromJolt(move);
            intent.look_dir       = MathBridge::FromJolt(fwd);
            intent.jump_requested = input.jump;
            intent.sprint_requested = false;
        });
}
