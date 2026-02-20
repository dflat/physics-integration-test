# RFC-0011: Audio System

* **Status:** Implemented
* **Date:** February 2026
* **Depends on:** RFC-0009 (Frame-Scoped Event Bus — `JumpEvent` and `LandEvent`
  are the triggering signals)

## Summary

Add an `AudioSystem` that consumes `JumpEvent` and `LandEvent` to play sound
effects, confirming the event bus works end-to-end with a real consumer. Audio
is handled by Raylib's built-in audio API — no additional library dependency.
Sound assets live in `resources/sounds/`. A dedicated `AudioResource` owns all
`Sound` (and future `Music`) handles, keeping audio concerns cleanly separated
from the shader/rendering assets in `AssetResource`.

## Motivation

`JumpEvent` and `LandEvent` have been emitting since RFC-0009, but nothing reads
them. The event bus is structurally tested in unit tests but has never had a real
in-game consumer. An audio system is the simplest possible consumer: it reads one
event type, calls one Raylib function, and produces an immediately verifiable
result. Completing this RFC validates the entire Pre-Update flush → CharacterState
emit → Logic consumer pipeline under real conditions.

Secondary benefit: jump and landing feedback makes the prototype significantly
more game-feel-complete for future iteration.

## Design

### Overview

1. **`InitAudioDevice()`** called in `main.cpp` after `InitWindow()`.
2. **`AudioResource`** (new, `src/audio_resource.hpp`) owns three `Sound` handles
   loaded from `resources/sounds/`. Structurally parallel to `AssetResource` but
   cleanly separated — audio assets and rendering/shader assets have different
   lifecycles and will scale differently.
3. **`AudioSystem`** is a stateless Logic-phase system with no `Register()` (no
   lifecycle hooks needed). `Update` reads `Events<JumpEvent>` and
   `Events<LandEvent>` from the World and calls `PlaySound()`.
4. `CloseAudioDevice()` called at shutdown before `CloseWindow()`.

### API

#### `AudioResource` (`src/audio_resource.hpp`)

```cpp
#pragma once
#include <raylib.h>

struct AudioResource {
    Sound snd_jump;    // first jump
    Sound snd_jump2;   // double jump (higher pitch cue)
    Sound snd_land;    // landing impact

    // Music bgm{};    // reserved for future background music streaming

    void load() {
        snd_jump  = LoadSound("resources/sounds/jump.wav");
        snd_jump2 = LoadSound("resources/sounds/jump2.wav");
        snd_land  = LoadSound("resources/sounds/land.wav");
    }

    void unload() {
        UnloadSound(snd_jump);
        UnloadSound(snd_jump2);
        UnloadSound(snd_land);
    }
};
```

`LoadSound()` returns a zeroed `Sound` if the file is missing; `PlaySound()` on
a zeroed Sound is a no-op. The system degrades gracefully during development
before final assets land.

**On `Register()`:** `AudioSystem` has no `Register()`. In this codebase
`Register()` means "install ECS lifecycle hooks (`on_add`/`on_remove`)". Asset
loading is a different concern and belongs in the explicit `load()`/`unload()`
pair on `AudioResource`, called from `main.cpp` — consistent with `AssetResource`.

**On background music:** Raylib's `Music` type is a streaming handle that
requires `UpdateMusicStream(music)` called every frame. When background music is
added, `AudioResource` gains a `Music bgm` field and `AudioSystem::Update` calls
`UpdateMusicStream`. No ECS or pipeline changes needed — it slots into the
existing system.

#### `AudioSystem` (`src/systems/audio.hpp` / `audio.cpp`)

```cpp
class AudioSystem {
public:
    // No Register() — no lifecycle hooks.
    static void Update(ecs::World& world, float dt);
};
```

`Update` logic:
```cpp
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
```

`try_resource` guards mean the system is safe even if the event bus or audio
resource is absent (e.g., in a future test harness).

### Sound Assets

Three WAV files in `resources/sounds/` (44100 Hz, 16-bit mono, synthesised):

| File | Character | Duration |
|------|-----------|----------|
| `jump.wav` | Upward pitch sweep ~300→600 Hz | ~120 ms |
| `jump2.wav` | Higher sweep ~500→1000 Hz | ~90 ms |
| `land.wav` | Low thud, exponential frequency/amplitude decay | ~160 ms |

Assets are committed to the repo. Production audio can replace them in-place
without code changes.

### Pipeline Change (`main.cpp`)

```cpp
InitWindow(1280, 720, "...");
InitAudioDevice();          // ← new; must follow InitWindow

// ...

AudioResource audio;
audio.load();
world.set_resource(audio);  // ← new

// Logic phase
pipeline.add_logic([](ecs::World& w, float dt) { CharacterStateSystem::Update(w, dt); });
pipeline.add_logic([](ecs::World& w, float dt) { AudioSystem::Update(w, dt); });  // ← new
pipeline.add_logic([](ecs::World& w, float)    { PlatformBuilderSystem::Update(w); });
pipeline.add_logic([](ecs::World& w, float dt) { CharacterMotorSystem::Update(w, dt); });

// Shutdown
world.resource<AudioResource>().unload();  // ← new
CloseAudioDevice();                        // ← new; before CloseWindow
CloseWindow();
```

### Files Changed

| Action | Path | Reason |
|--------|------|--------|
| Create | `src/audio_resource.hpp` | Dedicated AudioResource (Sound handles, load/unload) |
| Create | `src/systems/audio.hpp` | AudioSystem declaration |
| Create | `src/systems/audio.cpp` | AudioSystem::Update — event consumption |
| Modify | `src/main.cpp` | InitAudioDevice, resource setup, pipeline, shutdown |
| Modify | `CMakeLists.txt` | Add `audio.cpp` to `demo` only (not `unit_tests` — Raylib not linked there) |
| Create | `resources/sounds/jump.wav` | First jump sound |
| Create | `resources/sounds/jump2.wav` | Double jump sound |
| Create | `resources/sounds/land.wav` | Landing sound |

No new FetchContent dependency — Raylib already provides its audio module.

## Alternatives Considered

**Extend `AssetResource` with Sound fields.** `AssetResource` owns GPU/shader
state; `AudioResource` owns audio state. They have different scaling trajectories
(audio clips grow independently of shaders). Proactive separation avoids
refactoring when the audio library inevitably grows beyond three clips.

**`AudioSystem::Register()` for asset loading.** In this codebase `Register()`
means "install ECS lifecycle hooks." Overloading it with asset loading would
muddy that contract. The explicit `AudioResource::load()` called in `main.cpp`
is honest and consistent with the existing pattern.

**FMOD or OpenAL Soft.** Overkill for three one-shot sound effects. Adds a
library dependency. Revisit if spatialized 3D audio or DSP is required.

**`SoundTag` / `SoundRequest` components on entities.** Adds ECS indirection
for no benefit — audio playback is not entity-centric; there is no per-entity
sound state to track.

## Testing

Raylib's audio device is unavailable in the headless test target; `audio.cpp`
is linked to `demo` only. Testing is functional:

1. **First jump** — Space; hear the upward sweep once.
2. **Double jump** — Space twice mid-air; first sound then the higher sweep.
3. **Landing** — fall from height; hear the thud on contact.
4. **Scene reset (R)** — no orphaned sounds, no crash.
5. **Rapid jumps** — `PlaySound` on an in-progress Sound restarts it; verify no crash.

The event bus flow is unit-tested by the RFC-0009 `[events]` cases. This RFC
confirms end-to-end wiring only.

## Risks & Open Questions

**`PlaySound` restarts an in-progress sound.** If the player jumps very quickly
the jump sound cuts off. Acceptable for a prototype. Raylib 5.x `LoadSoundAlias`
enables multi-instance playback without duplicating the audio buffer when needed.

**WAV file size.** Three short mono WAVs ≈ 10–30 KB each. Negligible.

**Windows CI artifact.** The existing CMake `POST_BUILD` step copies the entire
`resources/` tree — `resources/sounds/` is included automatically.
