#pragma once
#include "../components.hpp"
#include "../debug_panel.hpp"
#include "../events.hpp"
#include "../pipeline.hpp"
#include "../systems/character_input.hpp"
#include "../systems/character_motor.hpp"
#include "../systems/character_state.hpp"
#include <ecs/ecs.hpp>
#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// CharacterModule
//
// Registers lifecycle hooks for the character systems, registers the event
// queues that CharacterStateSystem emits (JumpEvent, LandEvent), wires
// CharacterInputSystem and CharacterStateSystem into the Logic phase, and
// adds "Character" debug rows.
//
// install_motor() must be called AFTER AudioModule and BuilderModule to
// ensure CharacterMotorSystem is the final Logic step — it calls
// ExtendedUpdate on the Jolt character, which must complete before the
// fixed Physics step.
//
// Ordering summary:
//   CharacterModule::install      → logic: CharInput, CharState
//   AudioModule::install          → logic: Audio
//   BuilderModule::install        → logic: Builder
//   CharacterModule::install_motor → logic: CharMotor  (last)
// ---------------------------------------------------------------------------

struct CharacterModule {
    static void install(ecs::World& world, ecs::Pipeline& pipeline) {
        // Lifecycle hooks
        CharacterInputSystem::Register(world);
        CharacterStateSystem::Register(world);
        CharacterMotorSystem::Register(world);

        // Event queues owned by CharacterStateSystem (it is the emitter)
        world.resource<EventRegistry>().register_queue<JumpEvent>(world);
        world.resource<EventRegistry>().register_queue<LandEvent>(world);

        // Logic pipeline — CharInput then CharState
        pipeline.add_logic([](ecs::World& w, float dt) { CharacterInputSystem::Update(w, dt); });
        pipeline.add_logic([](ecs::World& w, float dt) { CharacterStateSystem::Update(w, dt); });

        // Debug rows
        if (auto* panel = world.try_resource<DebugPanel>()) {
            panel->watch("Character", "Mode", [&world]() {
                std::string r = "-";
                world.each<CharacterState>([&](ecs::Entity, CharacterState& s) {
                    r = (s.mode == CharacterState::Mode::Grounded) ? "Grounded" : "Airborne";
                });
                return r;
            });
            panel->watch("Character", "Jump Count", [&world]() {
                std::string r = "-";
                world.each<CharacterState>([&](ecs::Entity, CharacterState& s) {
                    r = std::to_string(s.jump_count);
                });
                return r;
            });
            panel->watch("Character", "Air Time", [&world]() {
                std::string r = "-";
                world.each<CharacterState>([&](ecs::Entity, CharacterState& s) {
                    char b[16];
                    std::snprintf(b, sizeof(b), "%.2f s", s.air_time);
                    r = b;
                });
                return r;
            });
        }
    }

    // Adds CharacterMotorSystem to the Logic phase.
    // Must be called after all other Logic-phase installs.
    static void install_motor(ecs::World& /*world*/, ecs::Pipeline& pipeline) {
        pipeline.add_logic([](ecs::World& w, float dt) { CharacterMotorSystem::Update(w, dt); });
    }
};
