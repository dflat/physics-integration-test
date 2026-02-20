#pragma once
#include <raylib.h>

// ---------------------------------------------------------------------------
// AudioResource — owns all audio clip and music-stream handles.
//
// Stored as a World resource. Loaded once at startup (after InitAudioDevice),
// unloaded at shutdown (before CloseAudioDevice).
//
// LoadSound() returns a zeroed Sound on missing file; PlaySound() on a zeroed
// Sound is a no-op, so the system degrades gracefully during development.
// ---------------------------------------------------------------------------

struct AudioResource {
    Sound snd_jump;     // first jump
    Sound snd_jump2;    // double jump (higher pitch)
    Sound snd_land;     // landing impact

    // Music bgm{};    // reserved: background music stream (UpdateMusicStream each frame)

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
