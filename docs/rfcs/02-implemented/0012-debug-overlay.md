# RFC-0012: Debug Overlay

* **Status:** Implemented
* **Date:** February 2026
* **Depends on:** RFC-0009 (Frame-Scoped Event Bus — `Events<T>` are inspectable
  via providers), RFC-0011 (AudioResource — establishes the dedicated-resource
  pattern this RFC follows)

## Summary

Add a toggleable on-screen debug overlay driven by a **provider registry**:
named `std::function<std::string()>` callbacks registered at startup, called
each render frame, and rendered as a sectioned text panel by a new
`DebugSystem`. The registry is open — any code with access to the `DebugPanel`
resource and the `World` can add rows without modifying existing systems. The
overlay is entirely opt-out: absent from production builds simply by not
registering providers or not adding `DebugSystem` to the pipeline.

## Motivation

As the engine grows, there is increasing need to inspect live system state
without attaching a debugger or scattering `printf` calls. Current pain points:

- Character state transitions (Grounded/Airborne, jump count) are invisible
  at runtime — reproducing edge-case bugs requires guesswork.
- Physics fixed-step timing, entity counts, and event throughput have no
  visibility.
- Adding temporary diagnostic logging pollutes system code and must be cleaned
  up manually.

A persistent overlay with a stable extension mechanism gives every future
system a standard place to expose diagnostic data — without touching any
system's logic path.

**Why not write into the panel from `Update` methods?** Systems run in the
Logic phase; the panel is consumed in the Render phase. Coordinating the
clear-and-repopulate cycle across phases creates ordering dependencies and
muddies system responsibilities. The provider pattern sidesteps this: providers
are registered once and called only during rendering, with no cross-phase
coupling.

## Design

### Core concept: the provider registry

```
Startup:  panel.watch("Character", "Mode", [&world]() { ... })
                                                  ↑
                               captures World& (safe: same lifetime)

Render:   DebugSystem::Update
            for each section → for each row → call provider() → render string
```

Providers are `std::function<std::string()>` — they can query the `World`,
read Raylib state (`GetFPS()`), or return any string. Registration is additive:
call `watch(section, label, fn)` from anywhere that has the `DebugPanel` and
relevant context. No existing system needs modification.

### `DebugPanel` (`src/debug_panel.hpp`)

Zero engine dependencies (no Raylib, no Jolt, no ECS headers required).

```cpp
#pragma once
#include <functional>
#include <string>
#include <vector>

struct DebugPanel {
    using Provider = std::function<std::string()>;

    struct Row {
        std::string label;
        Provider    fn;
    };

    struct Section {
        std::string      title;
        std::vector<Row> rows;
    };

    bool visible = false;

    // Register a named provider under a section heading.
    // Creates the section if it does not exist.
    void watch(const std::string& section,
               const std::string& label,
               Provider fn);

    const std::vector<Section>& sections() const { return sections_; }

private:
    std::vector<Section> sections_;
};
```

`watch()` is implemented in `debug_panel.cpp` (or inline in the header — no
Raylib dependency either way).

### `DebugSystem` (`src/systems/debug.hpp` / `debug.cpp`)

Raylib-dependent; linked to `demo` only.

```cpp
class DebugSystem {
public:
    // No Register() — no lifecycle hooks.
    static void Update(ecs::World& world, float dt);
};
```

`Update` logic:
1. Read `DebugPanel` resource; return early if absent.
2. Toggle `visible` on `KEY_F3`.
3. If not visible, return.
4. Call every registered provider to get its current string.
5. Render: semi-transparent background rectangle, section headers, label:value rows.

### Overlay layout

```
┌──────────────────────────┐
│  DEBUG  [F3 to hide]     │
├──────────────────────────┤
│  Engine                  │
│    FPS         60        │
│    Frame Time  16 ms     │
│    Entities    15        │
├──────────────────────────┤
│  Character               │
│    Mode        Grounded  │
│    Jump Count  0         │
│    Air Time    0.00 s    │
└──────────────────────────┘
```

Fixed position: top-left, 10 px margin. Background: `DARKGRAY` at 70% alpha.
Section titles in a lighter colour; rows in white. Default font size 10.

### Providers registered at startup (`main.cpp`)

```cpp
AudioResource audio; audio.load(); world.set_resource(audio);

DebugPanel panel;
panel.watch("Engine", "FPS",         []()      { return std::to_string(GetFPS()); });
panel.watch("Engine", "Frame Time",  []()      { return std::to_string((int)(GetFrameTime()*1000)) + " ms"; });
panel.watch("Engine", "Entities",    [&world]() {
    int n = 0; world.each([&](ecs::Entity){ ++n; }); return std::to_string(n);
});
panel.watch("Character", "Mode",      [&world]() {
    std::string r = "-";
    world.each<CharacterState>([&](CharacterState& s){
        r = (s.mode == CharacterState::Mode::Grounded) ? "Grounded" : "Airborne";
    });
    return r;
});
panel.watch("Character", "Jump Count", [&world]() {
    std::string r = "-";
    world.each<CharacterState>([&](CharacterState& s){ r = std::to_string(s.jump_count); });
    return r;
});
panel.watch("Character", "Air Time", [&world]() {
    std::string r = "-";
    world.each<CharacterState>([&](CharacterState& s){
        char buf[16]; std::snprintf(buf, sizeof(buf), "%.2f s", s.air_time);
        r = buf;
    });
    return r;
});
world.set_resource(std::move(panel));
```

### Files Changed

| Action | Path | Reason |
|--------|------|--------|
| Create | `src/debug_panel.hpp` | `DebugPanel` resource — zero engine deps |
| Create | `src/systems/debug.hpp` | `DebugSystem` declaration |
| Create | `src/systems/debug.cpp` | Toggle + provider dispatch + Raylib rendering |
| Modify | `src/main.cpp` | Construct `DebugPanel`, register providers, add `DebugSystem` to Render phase |
| Modify | `CMakeLists.txt` | Add `debug.cpp` to `demo` only |

No new library dependency. `debug_panel.hpp` has no engine includes; `debug.cpp`
uses Raylib already linked to `demo`.

## Alternatives Considered

**Systems write to the panel in `Update` (push model).** Each system checks
`world.try_resource<DebugPanel>()` and calls `panel.set(section, label, value)`
in its own `Update`. Simpler per-call, but mixes debug writes into logic paths,
creates an implicit frame-ordering dependency (clear-before-write must precede
all writers), and adds debug conditionals to every system. The provider pattern
is written once per row and invoked only in the Render phase.

**`DebugSystem` queries components directly with no registry.** Hardcodes which
components to display; adding new rows requires editing `DebugSystem`. Breaks
the open extension goal.

**ImGui.** Feature-rich but adds a significant dependency and requires
integration with the Raylib render loop. Overkill for the current scope;
`DrawText`-based overlay is sufficient and dependency-free.

**Compile-time strip (`#ifdef DEBUG_OVERLAY`).** Unnecessary — the `visible`
flag and `try_resource` guard mean zero runtime cost when the overlay is hidden,
and the system is simply omitted from a hypothetical production pipeline.

## Testing

`debug_panel.hpp` has no engine dependency and is testable headlessly:

- Unit: `watch()` with a stub provider; assert the section/label/value structure
  is correct.
- Unit: multiple sections and rows ordered correctly.

`debug.cpp` is Raylib-dependent (audio device + window required) and is
verified functionally:

1. Press **F3** → overlay appears; press again → disappears.
2. Jump: **Character → Mode** transitions Grounded → Airborne → Grounded.
3. Double-jump: **Jump Count** increments to 2.
4. Add a new `panel.watch(...)` call; confirm row appears without modifying
   any existing file except `main.cpp`.

## Risks & Open Questions

**Provider call cost.** Each provider is a `std::function` call + a
`world.each<T>` query per render frame. At 60 Hz with ~10 rows this is
negligible. If component queries become expensive (large archetypes), providers
can cache their last value and only re-query every N frames.

**Long strings / overflow.** The renderer uses a fixed left-column width for
labels; values longer than ~20 characters will overflow the panel. Values should
be kept short; no wrapping is implemented in v1.

**Multi-entity queries.** Providers that query a component present on multiple
entities (e.g., future multiplayer characters) will only show the last entity's
value. Acceptable for v1 — the panel is a single-entity debug tool at this
stage.
