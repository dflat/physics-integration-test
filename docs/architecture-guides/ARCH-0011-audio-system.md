# ARCH-0011: Audio System

* **RFC Reference:** [RFC-0011 — Audio System](../rfcs/02-implemented/0011-audio-system.md)
* **Implementation Date:** 2026-02-19
* **Status:** Active

---

## 1. High-Level Mental Model

> `CharacterStateSystem` emits events; `AudioSystem` hears them. The event bus
> is the contract — the audio system has no knowledge of characters, physics, or
> input. It reads a queue, plays a sound, and returns. Adding a new sound effect
> means adding a clip to `AudioResource` and a queue-read in `AudioSystem::Update`.

* **Core Responsibility:** Translate frame-scoped ECS events into Raylib
  `PlaySound()` calls.
* **Pipeline Phase:** Logic — immediately after `CharacterStateSystem::Update`
  (the emitter), so events are live in the queue when the audio system runs.
* **Key Constraint:** `InitAudioDevice()` must be called before
  `AudioResource::load()`, and `CloseAudioDevice()` must be called after
  `AudioResource::unload()`. Both are explicit in `main.cpp`.

---

## 2. The Grand Tour

| File | Role |
|------|------|
| `src/audio_resource.hpp` | `AudioResource` — owns `Sound` handles; `load()`/`unload()`. |
| `src/systems/audio.hpp` | `AudioSystem` declaration. |
| `src/systems/audio.cpp` | `AudioSystem::Update` — reads event queues, calls `PlaySound()`. |
| `resources/sounds/jump.wav` | First jump: upward sweep 300→600 Hz, 120 ms. |
| `resources/sounds/jump2.wav` | Double jump: higher sweep 500→1000 Hz, 90 ms. |
| `resources/sounds/land.wav` | Landing impact: low thud, exponential decay, 160 ms. |

### No Register(), No Unit Tests

`AudioSystem` has no `Register()` because it installs no ECS lifecycle hooks.
`AudioResource::load()` is called explicitly in `main.cpp` — the same pattern
as `AssetResource`. There are no unit tests for `audio.cpp` because Raylib's
audio device is unavailable in the headless test target; validation is
functional (run the demo, hear sounds).

---

## 3. Components & Data Ownership

`AudioSystem` does not own or write any ECS components. It is a pure consumer:

| Input | Source |
|-------|--------|
| `Events<JumpEvent>` | World resource; written by `CharacterStateSystem` |
| `Events<LandEvent>` | World resource; written by `CharacterStateSystem` |
| `AudioResource` | World resource; loaded in `main.cpp`, read-only by `AudioSystem` |

### `AudioResource` layout

```cpp
struct AudioResource {
    Sound snd_jump;     // first jump
    Sound snd_jump2;    // double jump
    Sound snd_land;     // landing impact
    // Music bgm{};    // reserved for background music streaming
};
```

`Sound` is a Raylib handle (small POD). `LoadSound()` with a missing file
returns a zeroed struct; `PlaySound()` on it is a no-op — graceful degradation
while assets are in flux.

### `AudioResource` vs `AssetResource`

| | `AssetResource` | `AudioResource` |
|---|---|---|
| Contents | Shaders, GPU uniform locations | Audio clips, music streams |
| Raylib API | `LoadShader`, `SetShaderValue` | `LoadSound`, `PlaySound` |
| Init dependency | `InitWindow()` | `InitAudioDevice()` (after `InitWindow`) |
| Scaling trajectory | Grows with visual complexity | Grows with audio content |

Kept separate because they scale independently and have different init
dependencies. Merging them would require every shader change to reason about
audio and vice versa.

---

## 4. Data Flow

```
Pre-Update:  EventRegistry::flush_all()    ← clears JumpEvent / LandEvent queues

Logic:       CharacterStateSystem::Update()
               → detects jump / land transitions
               → Events<JumpEvent>::send() / Events<LandEvent>::send()

             AudioSystem::Update()
               → reads Events<JumpEvent>  → PlaySound(snd_jump or snd_jump2)
               → reads Events<LandEvent>  → PlaySound(snd_land)
```

Events produced by `CharacterStateSystem` and consumed by `AudioSystem` live
exactly one frame — they are populated during Logic, consumed by the end of
Logic, and flushed at the start of the next Pre-Update.

---

## 5. System Integration (The Social Map)

**Upstream:** `CharacterStateSystem` — sole emitter of `JumpEvent` and
`LandEvent`. `AudioSystem` is decoupled from it; if the character system is
refactored to emit differently, `AudioSystem` sees no change as long as the
event types are preserved.

**No downstream:** `AudioSystem` writes nothing. It is a terminal consumer.

**Adding new sounds:** The pattern is:
1. Add a `Sound snd_foo` field to `AudioResource`; load/unload it there.
2. Define a new event type in `events.hpp`; register its queue in `main.cpp`.
3. Add an emitter in the appropriate system.
4. Add a `read()` loop in `AudioSystem::Update`.

No ECS changes required. No new pipeline phases.

**Background music (future):** Add `Music bgm` to `AudioResource`. Load with
`LoadMusicStream()`, unload with `UnloadMusicStream()`. In `AudioSystem::Update`,
call `UpdateMusicStream(audio->bgm)` unconditionally each frame. No new
systems, no new pipeline phases.

---

## 6. Trade-offs & Gotchas

**`PlaySound` restarts an in-progress sound.** Two rapid jumps cut the first
sound. Acceptable for a prototype. `LoadSoundAlias` (Raylib 5.x) enables
multi-instance playback without duplicating the audio buffer when polyphony
matters.

**WAV files are synthesised placeholders.** Replaced in-place with production
assets without any code changes — the paths are the only contract.

**`audio.cpp` is not in `unit_tests`.** Raylib requires an audio device;
headless test builds cannot call `InitAudioDevice`. The event bus itself is
tested in the `[events]` Catch2 cases; `AudioSystem` is verified functionally.

---

## 7. Related Resources

* **RFC:** [`docs/rfcs/02-implemented/0011-audio-system.md`](../rfcs/02-implemented/0011-audio-system.md)
* **AudioResource:** `src/audio_resource.hpp`
* **AudioSystem:** `src/systems/audio.hpp`, `src/systems/audio.cpp`
* **Sound files:** `resources/sounds/`
* **Event bus:** `src/events.hpp` (RFC-0009 / ARCH-0009)
* **ARCH_STATE.md:** Section 4 (System Responsibilities)
