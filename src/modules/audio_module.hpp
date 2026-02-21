#pragma once
#include "../audio_resource.hpp"
#include "../events.hpp"
#include "../pipeline.hpp"
#include "../systems/audio.hpp"
#include <ecs/ecs.hpp>
#include <raylib.h>

// ---------------------------------------------------------------------------
// AudioModule
//
// Initialises Raylib's audio device, loads the AudioResource (Sound handles),
// and adds AudioSystem to the Logic phase.
//
// Pipeline placement: AudioSystem must run after CharacterStateSystem
// (which emits JumpEvent / LandEvent) and before CharacterMotorSystem.
// Callers must respect this by installing AudioModule between
// CharacterModule::install and CharacterModule::install_motor.
//
// shutdown() unloads sounds and closes the audio device. Must be called
// before CloseWindow().
// ---------------------------------------------------------------------------

struct AudioModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        InitAudioDevice();
        AudioResource audio;
        audio.load();
        world.set_resource(std::move(audio));
        pipeline.add_logic([](ecs::World& w, float dt) { AudioSystem::Update(w, dt); });
    }

    static void shutdown(ecs::World& world) {
        world.resource<AudioResource>().unload();
        CloseAudioDevice();
    }
};
