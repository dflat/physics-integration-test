# RFC-0010: Scene Serialization

* **Status:** Implemented
* **Date:** February 2026
* **Depends on:** RFC-0008 (Physics Handle Isolation — `components.hpp` is now
  engine-free, making `scene.cpp` headlessly compilable and testable)

## Summary

Replace the hardcoded `SpawnScene()` function in `main.cpp` with a JSON scene
loader. Scenes are defined in `resources/scenes/*.json`, parsed by a new
`SceneLoader` module, and loaded at startup and on scene reset. The loader
creates ECS entities directly from JSON data, triggering the existing lifecycle
hooks (`on_add<RigidBodyConfig>`, `on_add<CharacterControllerConfig>`, etc.)
exactly as the handwritten `SpawnScene()` did. The current scene becomes
`resources/scenes/default.json`.

## Motivation

The hardcoded `SpawnScene()` has three problems:

**1. Level design requires recompiling.** Adding a platform, adjusting a
position, or changing a color means editing `main.cpp` and rebuilding. A JSON
file can be edited in any text editor and reloaded without touching C++.

**2. The scene definition is coupled to engine code.** `SpawnScene()` uses
Raylib math helpers (`QuaternionFromAxisAngle`, `DEG2RAD`) to compute
quaternions. The scene description and the runtime construction are tangled.

**3. There is no scene reset path.** The R-key reset loop in `main.cpp` inline
duplicates scene spawning logic (`to_destroy` vector, destroy loop,
`SpawnScene` call). With a `SceneLoader` the reset becomes two lines:
`SceneLoader::unload(world); SceneLoader::load(world, path)`.

The ECS library's binary `serialize`/`deserialize` is intentionally not used
here. That format requires all component types to have serialize/deserialize
functions — including runtime handles (`RigidBodyHandle`, `CharacterHandle`),
which hold live Jolt pointers that cannot be meaningfully serialized. Binary
snapshots are the right tool for save/load game state; JSON is the right tool
for scene authoring.

## Design

### `SceneLoader` API

```cpp
class SceneLoader {
public:
    // Load entities from a JSON file. Returns false if file is not found
    // or JSON is malformed; true otherwise.
    static bool load(ecs::World& world, const std::string& path);

    // Parse and spawn from a JSON string. Identical to load() but skips
    // file I/O — used for unit testing without a filesystem.
    static bool load_from_string(ecs::World& world, const std::string& json);

    // Destroy all WorldTag entities and flush deferred commands.
    static void unload(ecs::World& world);
};
```

### JSON schema

```json
{
  "entities": [
    {
      "transform": {
        "position": [x, y, z],
        "rotation": [x, y, z, w],
        "scale":    [x, y, z]
      },
      "mesh": {
        "shape":        "Box | Sphere | Capsule",
        "color":        [r, g, b, a],
        "scale_offset": [x, y, z]
      },
      "box_collider":    { "half_extents": [x, y, z] },
      "sphere_collider": { "radius": r },
      "rigid_body": {
        "type":        "Static | Dynamic | Kinematic",
        "mass":        1.0,
        "friction":    0.5,
        "restitution": 0.0,
        "sensor":      false
      },
      "character": {
        "height":          1.8,
        "radius":          0.4,
        "mass":            70.0,
        "max_slope_angle": 45.0
      },
      "tags": ["World", "Player"]
    }
  ]
}
```

All fields are optional. Omitted sub-fields use component defaults. The
`"Player"` tag adds `PlayerTag`, `PlayerInput`, and `PlayerState` components.

### Component add ordering

Lifecycle hooks (`on_add<RigidBodyConfig>`, `on_add<CharacterControllerConfig>`)
read sibling components from the same entity to configure the Jolt body:
- `PhysicsSystem` reads `BoxCollider`/`SphereCollider` and `LocalTransform`
- `CharacterMotorSystem` reads `LocalTransform` and `CharacterControllerConfig`

The loader always adds components in this order per entity:
1. `LocalTransform`, `WorldTransform`
2. `BoxCollider` / `SphereCollider`
3. `MeshRenderer`
4. `RigidBodyConfig` or `CharacterControllerConfig`  ← triggers hooks
5. Tags (`WorldTag`, `PlayerTag`, `PlayerInput`, `PlayerState`)

### Dependency: nlohmann/json

Added via FetchContent (v3.11.3). Header-only; adds no link-time dependency
beyond the include path. Linked to both `demo` and `unit_tests` targets so
that `scene.cpp` is available in headless tests.

### Files Changed

| Action | Path | Reason |
|--------|------|--------|
| Create | `src/scene.hpp` | `SceneLoader` declaration |
| Create | `src/scene.cpp` | JSON parsing and entity creation |
| Create | `resources/scenes/default.json` | Current hardcoded scene as JSON |
| Modify | `CMakeLists.txt` | Add nlohmann/json FetchContent; add scene.cpp to both targets |
| Modify | `src/main.cpp` | Remove `SpawnScene()`; call `SceneLoader::load/unload` |
| Modify | `tests/logic_tests.cpp` | SceneLoader unit tests (headless, no Jolt) |

## Alternatives Considered

**Binary ECS snapshot (`ecs::serialize`/`ecs::deserialize`).** Requires all
component types to expose custom serialize/deserialize functions. Runtime
handles (`RigidBodyHandle`, `CharacterHandle`) cannot be meaningfully
serialized. Wrong tool for scene authoring; appropriate for save/load state.

**TOML or custom format.** JSON is simpler for both parsing (nlohmann/json)
and editing (universally understood). TOML adds a library without meaningful
benefit at this scale.

**Keep `SpawnScene()`, add JSON as optional override.** Dual-path maintenance.
Clean break is better: the JSON becomes the single source of truth for scene
content.

## Testing

- `SceneLoader::load_from_string` is testable headlessly:  a bare `ecs::World`
  (no systems registered) can receive `world.add()` calls without any Jolt or
  Raylib involvement. Tests verify entity count and sampled component values.
- `scene.cpp` has no Jolt or Raylib includes; it compiles in the `unit_tests`
  target.
- Functional: the demo must load identically to the previous hardcoded scene.
