# RFC-0014: Platform Builder Raycast & Layer Filter Convention

* **Status:** Implemented
* **Date:** February 2026

## Summary

Extends `PlatformBuilderSystem` with a downward Jolt `NarrowPhaseQuery::CastRay`
that detects existing static geometry in the platform's spawn volume. If geometry
is found, the new platform is snapped on top of it rather than spawning inside it.
This RFC also documents the non-obvious but correct way to configure Jolt layer
filters for one-off utility queries, which differs from the standard body-vs-body
collision pair setup.

## Motivation

### The spawn-overlap problem

Before this fix, the platform spawn position was computed as:

```cpp
ecs::Vec3 spawn_pos = { player_pos.x, player_pos.y - 0.65f, player_pos.z };
```

The offset `0.65f = char_radius(0.4) + platform_half_height(0.25)` places the
platform's top surface exactly at the character's feet, which prevents the Jolt
`CharacterVirtual` being pushed upward when building in mid-air. However, when
the player is standing on a surface (the ground, or a previously built platform),
the same calculation places the new platform *inside* that surface. Two static
bodies overlapping produce no collision response from Jolt — they simply coexist,
producing intersecting visible geometry.

### First attempt: wrong broadphase layer

The initial fix tried a downward ray using the `NON_MOVING` object layer for the
broadphase filter:

```cpp
JPH::DefaultBroadPhaseLayerFilter bp_filter(
    ctx.object_vs_broadphase_layer_filter, Layers::NON_MOVING);  // BUG
JPH::DefaultObjectLayerFilter obj_filter(
    ctx.object_layer_pair_filter, Layers::NON_MOVING);           // BUG
```

This produced no hits. The ray appeared to pass straight through the ground and
every static platform. The root cause is explained in the Design section below.

## Design

### Why the standard pair-filter breaks for utility queries

Our collision layer setup follows the canonical Jolt example:

```cpp
// ObjectVsBroadPhaseLayerFilterImpl
NON_MOVING -> only collides with BroadPhaseLayers::MOVING
MOVING     -> collides with everything
```

This is correct for *body-vs-body simulation*: there is no point generating
collision pairs between two static bodies, so static-vs-static is filtered out
at the broadphase level.

`DefaultBroadPhaseLayerFilter` reuses this same filter for *queries*:

```cpp
// DefaultBroadPhaseLayerFilter::ShouldCollide calls:
mFilter.ShouldCollide(mLayer, broadphase_layer_of_candidate)
//      ^^^^^^^^ the ObjectVsBroadPhaseLayerFilter we set up for bodies
```

So when we pass `Layers::NON_MOVING` as the "casting layer":

```
ShouldCollide(NON_MOVING, BroadPhaseLayers::NON_MOVING)
    -> inLayer2 == BroadPhaseLayers::MOVING? NO -> FALSE
```

The broadphase rejects the entire NON_MOVING bucket. Static bodies (ground,
platforms) live in `BroadPhaseLayers::NON_MOVING`, so they are never tested.
The ray misses everything, regardless of what the narrow-phase object filter says.

Similarly, `DefaultObjectLayerFilter` with `NON_MOVING`:

```
ShouldCollide(NON_MOVING, candidate_layer)
    -> candidate_layer == MOVING? Only true for dynamic bodies
```

Both filters are the wrong tool for utility queries. They are designed to answer
"can object A collide with object B in the simulation?" not "should this ray test
against body B?"

### The correct approach

Decouple the utility query from the pair-filter table entirely. Use:

1. **Broadphase filter:** `Layers::MOVING` — because MOVING collides with all
   broadphase layers, the ray searches both the NON_MOVING and MOVING broadphase
   buckets, reaching static bodies.
2. **Object layer filter:** a bespoke `ObjectLayerFilter` subclass that accepts
   only `NON_MOVING` object layers, ignoring dynamic bodies.

```cpp
// src/systems/builder.cpp

struct StaticOnlyLayerFilter final : public JPH::ObjectLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer == Layers::NON_MOVING;
    }
};
```

Usage:

```cpp
JPH::DefaultBroadPhaseLayerFilter bp_filter(
    ctx.object_vs_broadphase_layer_filter, Layers::MOVING);  // sees all BP layers
StaticOnlyLayerFilter obj_filter;                            // static bodies only
JPH::BodyFilter body_filter;                                 // no body exclusions

JPH::RRayCast ray{
    JPH::RVec3(origin.x, origin.y, origin.z),
    JPH::Vec3(0.f, -ray_length, 0.f)
};
JPH::RayCastResult result;

bool hit = ctx.physics_system->GetNarrowPhaseQuery()
    .CastRay(ray, result, bp_filter, obj_filter, body_filter);
```

If `hit`, `result.mFraction` is in [0, 1] along `ray.mDirection`. The hit
position is `ray.mOrigin + result.mFraction * ray.mDirection`.

### Snap-to-surface calculation

```cpp
constexpr float k_char_radius     = 0.4f;   // mSupportingVolume offset
constexpr float k_platform_half_h = 0.25f;  // half of platform height 0.5

float feet_y  = player_pos.y - k_char_radius;
float spawn_y = feet_y - k_platform_half_h;  // default: top at feet

JPH::RRayCast ray{
    JPH::RVec3(player_pos.x, feet_y, player_pos.z),
    JPH::Vec3(0.f, -(size.y + 0.01f), 0.f)  // down by full platform height + epsilon
};
JPH::RayCastResult result;
// ... filters ...

if (ctx.physics_system->GetNarrowPhaseQuery()
        .CastRay(ray, result, bp_filter, obj_filter, body_filter)) {
    float surface_top   = feet_y + result.mFraction * ray.mDirection.GetY();
    float surface_based = surface_top + k_platform_half_h;
    spawn_y = std::max(spawn_y, surface_based);
}
```

- **In mid-air:** ray misses → `spawn_y` stays at the default (top flush with
  feet). No displacement of the character.
- **On a surface:** ray hits → `surface_top` is the Y coordinate of the
  surface's top face. `spawn_y` is raised so the platform's bottom sits on that
  surface, with zero overlap.

`std::max` handles the degenerate case where the ray hits something above the
default position (e.g. building on an overhanging ledge) gracefully.

## Alternatives Considered

### Wait for Jolt to resolve the overlap

Two static bodies never generate a contact constraint in Jolt — only dynamic
bodies respond to penetration. Relying on collision resolution is not possible
for static-vs-static.

### Use `CollideShape` or `CollidePoint`

`NarrowPhaseQuery::CollideShape` tests a shape volume against the world and
returns all contacts. This would work but is heavier than a single ray: it
generates a full contact manifold and allocates collector results. For a
player-triggered, cooldown-gated action a ray is more appropriate.

### Use a custom `BroadPhaseLayerFilter` instead of piggy-backing on MOVING

The fully correct idiom is a dedicated broadphase filter:

```cpp
struct AllBroadPhaseFilter final : public JPH::BroadPhaseLayerFilter {
    bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; }
};
```

Paired with `StaticOnlyLayerFilter` this makes intent fully explicit and is
immune to future changes in the layer table. The `MOVING`-piggyback approach
chosen for this commit works correctly with the current two-layer setup and is
marginally simpler. If a third layer is added in the future (RFC or otherwise),
this should be revisited. See also the Jolt guide (`docs/guides/jolt-physics.md`)
for the canonical pattern.

## Testing

Manual gameplay verification:
- Building in mid-air: platform spawns at feet height, no character displacement.
- Building on the ground: platform spawns flush on top of ground, no geometry
  intersection visible from any camera angle.
- Building on an existing platform: platform spawns flush on top of that
  platform, stacking correctly.
- Rapid building (holding trigger through the cooldown): no accumulated drift.

No automated test is included; the behaviour involves the live Jolt simulation
and is not exercisable in the headless test target.

## Risks & Open Questions

- **Layer table coupling:** The `Layers::MOVING` broadphase workaround silently
  breaks if `ObjectVsBroadPhaseLayerFilter` is ever changed so that MOVING no
  longer collides with all broadphase layers. Documented in Alternatives above.
  Mitigation: if layers are ever extended, replace with `AllBroadPhaseFilter`.

- **Query timing:** The ray is cast in the Logic phase, before the Physics phase
  tick. Bodies spawned via `world.deferred()` in the same frame are not yet
  present in Jolt when the ray is cast. This is correct behaviour (a platform
  built this frame should not affect the spawn position of a platform built next
  frame), but is worth noting.
