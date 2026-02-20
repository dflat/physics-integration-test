# ARCH-0010: Scene Serialization

* **RFC Reference:** [RFC-0010 — Scene Serialization](../rfcs/02-implemented/0010-scene-serialization.md)
* **Implementation Date:** 2026-02-19
* **Status:** Active

---

## 1. High-Level Mental Model

> The scene is the data; `main.cpp` is the wiring. Before this RFC, `main.cpp`
> was both. Now `main.cpp` calls `SceneLoader::load(world, path)` and the JSON
> file owns the scene content — positions, colors, physics configs, tags.
> Adding a platform means editing a JSON file, not recompiling.

* **Core Responsibility:** Parse `resources/scenes/*.json` and instantiate ECS
  entities with the correct components, in lifecycle-safe order.
* **Pipeline Phase:** Not a system — called once at startup and once per scene
  reset (R key). No per-frame involvement.
* **Key Constraint:** Components are added in a fixed order per entity so that
  `on_add` lifecycle hooks see their sibling components already present.

---

## 2. The Grand Tour

| File | Role |
|------|------|
| `src/scene.hpp` | `SceneLoader` declaration — `load`, `load_from_string`, `unload`. |
| `src/scene.cpp` | JSON parsing (nlohmann/json) and entity creation. No Jolt or Raylib dependency. |
| `resources/scenes/default.json` | The default parkour scene — 13 entities (ground + player + 11 platforms). |
| `src/main.cpp` | Calls `SceneLoader::load/unload`. `SpawnScene()` and `ToEcs()` removed. |
| `tests/logic_tests.cpp` | Headless tests for `load_from_string`: count, transform values, configs, error handling. |

### Lifecycle vs. Per-Frame Work

**Startup (called once in `main`):**
```cpp
SceneLoader::load(world, "resources/scenes/default.json");
```

**Scene reset (R key):**
```cpp
SceneLoader::unload(world);   // destroys all WorldTag entities + deferred flush
SceneLoader::load(world, SCENE_PATH);
```

No per-frame work. `SceneLoader` is a pure utility — no state, no registration.

---

## 3. Components & Data Ownership

### JSON → Component mapping

| JSON field | Components added |
|------------|-----------------|
| `"transform"` | `ecs::LocalTransform`, `ecs::WorldTransform` |
| `"mesh"` | `MeshRenderer` |
| `"box_collider"` | `BoxCollider` |
| `"sphere_collider"` | `SphereCollider` |
| `"rigid_body"` | `RigidBodyConfig` ← triggers `PhysicsSystem::on_add` |
| `"character"` | `CharacterControllerConfig` ← triggers 3 system `on_add` hooks |
| `"tags": ["World"]` | `WorldTag` |
| `"tags": ["Player"]` | `PlayerTag`, `PlayerInput`, `PlayerState` |

### Add ordering (critical)

```
For each entity in JSON:
  1. LocalTransform, WorldTransform    ← must be first
  2. BoxCollider / SphereCollider      ← before rigid_body
  3. MeshRenderer
  4. RigidBodyConfig                   ← triggers hook; reads collider + transform
     OR CharacterControllerConfig      ← triggers hook; reads transform
  5. Tags (WorldTag, PlayerTag, ...)
```

If `RigidBodyConfig` is added before `BoxCollider`, `PhysicsSystem`'s hook
falls back to a default box shape and ignores the collider. If `LocalTransform`
is missing when `CharacterMotorSystem`'s hook fires, the character spawns at
the origin.

### JSON format reference

```json
{
  "entities": [
    {
      "_name": "optional debug label (ignored by loader)",
      "transform": {
        "position": [x, y, z],
        "rotation": [x, y, z, w],
        "scale":    [x, y, z]
      },
      "mesh":          { "shape": "Box|Sphere|Capsule", "color": [r,g,b,a], "scale_offset": [x,y,z] },
      "box_collider":    { "half_extents": [x, y, z] },
      "sphere_collider": { "radius": r },
      "rigid_body":    { "type": "Static|Dynamic|Kinematic", "mass": 1.0, "friction": 0.5, "restitution": 0.0, "sensor": false },
      "character":     { "height": 1.8, "radius": 0.4, "mass": 70.0, "max_slope_angle": 45.0 },
      "tags":          ["World", "Player"]
    }
  ]
}
```

All fields and sub-fields are optional. Omitted transform sub-fields use
component defaults (position `{0,0,0}`, rotation `{0,0,0,1}`, scale `{1,1,1}`).

---

## 4. Data Flow

```
Startup / R-key reset:

  SceneLoader::unload(world)
    world.each<WorldTag> → collect entities
    world.destroy(e) for each
    world.deferred().flush(world)     ← triggers on_remove hooks (body cleanup)

  SceneLoader::load(world, path)
    std::ifstream → load_from_string()
    json::parse(content)
    for each entity in json["entities"]:
      world.create()
      world.add(LocalTransform, WorldTransform)
      world.add(BoxCollider)              if present
      world.add(MeshRenderer)            if present
      world.add(RigidBodyConfig)         if present → on_add fires → Jolt body created
      world.add(CharacterControllerConfig) if present → 3 on_add hooks fire
      world.add(WorldTag / PlayerTag / ...)
```

---

## 5. System Integration (The Social Map)

**`SceneLoader` has no upstream or downstream systems** — it creates entities
that systems then process normally. The lifecycle hooks (`on_add` callbacks
installed by `PhysicsSystem::Register`, `CharacterMotorSystem::Register`, etc.)
fire synchronously during `world.add()`, so by the time `load()` returns,
every entity has its runtime handles fully initialized.

**`SceneLoader::unload`** uses `WorldTag` as the sentinel for scene-owned
entities. PlatformBuilderSystem spawns entities with `WorldTag`, so those are
cleaned up on reset too — no special handling needed.

**Future scenes** are just additional JSON files in `resources/scenes/`. No
code changes are required to load a different scene; just pass a different path
to `SceneLoader::load`.

---

## 6. Trade-offs & Gotchas

**Quaternions are stored as `[x, y, z, w]`.** The Raylib helper
`QuaternionFromAxisAngle({axis}, angle_rad)` that the old `SpawnScene()` used
has been pre-computed into the JSON. For the 20° pitch ramp:
`[sin(10°), 0, 0, cos(10°)]` = `[0.17365, 0, 0, 0.98481]`.

**`_name` fields are silently ignored.** The loader uses `contains()` checks
for known fields; unknown fields (like `_name`) are simply skipped. This is
intentional and allows comments/labels in the JSON without a parser change.

**ECS binary `serialize`/`deserialize` is not used for scene authoring.**
It requires custom serialize/deserialize functions for all component types
including runtime handles (`RigidBodyHandle`, `CharacterHandle`) which hold
live Jolt pointers. That format is appropriate for future runtime save/load
(game state snapshots), not scene authoring.

**`load_from_string` does not call `deferred().flush()`.** The lifecycle hooks
fire synchronously from `world.add()`, so no explicit flush is needed after
loading. The flush in `unload()` is for the destroy path, which goes through
the deferred command buffer.

**`world.add(ent, cfg)` requires an rvalue.** Local `cfg` variables must be
passed as `std::move(cfg)`. Passing an lvalue reference causes a template
deduction error ("forming pointer to reference type").

---

## 7. Related Resources

* **RFC:** [`docs/rfcs/02-implemented/0010-scene-serialization.md`](../rfcs/02-implemented/0010-scene-serialization.md)
* **Loader:** `src/scene.hpp`, `src/scene.cpp`
* **Default scene:** `resources/scenes/default.json`
* **Tests:** `tests/logic_tests.cpp` — `[scene]` test cases
* **ARCH_STATE.md:** Section 5 (Scene Format)
