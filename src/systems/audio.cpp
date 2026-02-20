#include "audio.hpp"
#include "../audio_resource.hpp"
#include "../events.hpp"
#include <raylib.h>

void AudioSystem::Update(ecs::World& world, float /*dt*/) {
    auto* audio = world.try_resource<AudioResource>();
    if (!audio) return;

    if (const auto* evts = world.try_resource<Events<JumpEvent>>()) {
        for (const auto& ev : evts->read()) {
            Sound& s = (ev.jump_number == 1) ? audio->snd_jump : audio->snd_jump2;
            PlaySound(s);
        }
    }

    if (const auto* evts = world.try_resource<Events<LandEvent>>()) {
        if (!evts->empty())
            PlaySound(audio->snd_land);
    }
}
