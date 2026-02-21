# Jolt Physics — Developer Guide

A complete reference for developers working with Jolt Physics inside this engine.
The guide begins with general physics concepts for developers unfamiliar with the
domain, then covers Jolt's architecture, API, and the specific patterns used in
this codebase. After reading it you should be able to create bodies, write
queries, and extend the physics system without surprises.

---

## Table of Contents

1. [What a Physics Engine Does](#1-what-a-physics-engine-does)
2. [Core Concepts](#2-core-concepts)
   - 2.1 Rigid Bodies
   - 2.2 Shapes and Collision Geometry
   - 2.3 Broadphase and Narrowphase
   - 2.4 Collision Response
   - 2.5 Constraints
   - 2.6 The Fixed Timestep
3. [Jolt Architecture](#3-jolt-architecture)
   - 3.1 Initialization
   - 3.2 PhysicsSystem
   - 3.3 TempAllocator
   - 3.4 JobSystem
   - 3.5 BodyInterface
   - 3.6 Shapes and ShapeSettings
4. [Collision Layers — Jolt's Filtering System](#4-collision-layers--jolts-filtering-system)
   - 4.1 Two Dimensions of Filtering
   - 4.2 ObjectLayer
   - 4.3 BroadPhaseLayer
   - 4.4 The Three Filter Interfaces
   - 4.5 Our Two-Layer Setup
   - 4.6 Why Pair Filters Break for Utility Queries
   - 4.7 The Correct Pattern for Utility Queries
5. [Body Lifecycle](#5-body-lifecycle)
   - 5.1 BodyCreationSettings
   - 5.2 Creating and Adding Bodies
   - 5.3 Motion Types
   - 5.4 Activation and Sleeping
   - 5.5 Removing and Destroying Bodies
6. [Querying the World](#6-querying-the-world)
   - 6.1 BroadPhaseQuery vs NarrowPhaseQuery
   - 6.2 CastRay
   - 6.3 CollideShape
   - 6.4 CastShape
   - 6.5 Writing Correct Filters for Queries
7. [CharacterVirtual](#7-charactervirtual)
   - 7.1 Why Not a Rigid Body?
   - 7.2 CharacterVirtualSettings
   - 7.3 mSupportingVolume and Ground Detection
   - 7.4 ExtendedUpdate
   - 7.5 Velocity and Rotation
8. [Thread Safety](#8-thread-safety)
9. [How We Use Jolt in This Engine](#9-how-we-use-jolt-in-this-engine)
   - 9.1 PhysicsContext Resource
   - 9.2 MathBridge
   - 9.3 Body Creation via on_add Hooks
   - 9.4 The Physics Phase and Fixed Step
   - 9.5 Transform Synchronisation
   - 9.6 CharacterMotorSystem
   - 9.7 PlatformBuilderSystem — Raycast Pattern
10. [Recipes](#10-recipes)
    - 10.1 Static Box Collider
    - 10.2 Dynamic Rigid Body
    - 10.3 Character Controller
    - 10.4 Raycast Against Static Geometry Only
    - 10.5 Raycast Against All Geometry
    - 10.6 Sensor / Trigger Volume
11. [Common Pitfalls](#11-common-pitfalls)

---

## 1. What a Physics Engine Does

A physics engine is responsible for:

- **Collision detection** — determining if and where objects overlap.
- **Collision response** — computing forces and impulses to separate objects and
  simulate realistic momentum exchange.
- **Integration** — advancing positions and velocities through time under forces
  (gravity, applied forces, constraints).
- **Queries** — answering questions like "what does this ray hit?" or "what
  objects overlap this sphere?" independently of the simulation.

Jolt is a *rigid-body* physics engine. It treats every object as perfectly
rigid — it cannot deform. This is the right model for environmental geometry,
projectiles, props, and most gameplay objects. Soft bodies (cloth, ropes) and
fluid simulation are out of scope.

---

## 2. Core Concepts

### 2.1 Rigid Bodies

A rigid body is an object with:

- **Position** — where its centre of mass is in the world.
- **Orientation** — its rotation, stored as a quaternion.
- **Linear velocity** — how fast the centre of mass is moving.
- **Angular velocity** — how fast it is spinning, stored as a vector (axis ×
  angular speed).
- **Mass** and **inertia tensor** — determine how hard it is to push or spin.
- **Shape** — the collision geometry attached to it.

The simulation integrates linear and angular velocities over time, applies
gravity, resolves contacts, and updates positions.

### 2.2 Shapes and Collision Geometry

Every body has a **shape** that defines its collision surface. Shapes are
separate objects from bodies — the same shape can be shared by many bodies.
Jolt provides primitives:

| Shape | Constructor |
|-------|-------------|
| `BoxShape` | Half-extents vector |
| `SphereShape` | Radius |
| `CapsuleShape` | Half-height of cylinder part, radius |
| `CylinderShape` | Half-height, radius |
| `ConvexHullShape` | Array of vertices |
| `MeshShape` | Triangle soup (static only) |
| `HeightFieldShape` | Terrain grid |
| `RotatedTranslatedShape` | Wraps any shape with a local offset/rotation |
| `OffsetCenterOfMassShape` | Shifts the centre of mass |

Shapes are created via a **ShapeSettings** → **Shape** two-step:

```cpp
// ShapeSettings is serialisable and can be shared.
// Shape is the runtime object the simulation uses.
JPH::BoxShapeSettings settings(JPH::Vec3(1.0f, 0.5f, 2.0f));
auto result = settings.Create();
if (!result.HasError()) {
    JPH::RefConst<JPH::Shape> shape = result.Get();
}

// For simple cases, construct a Shape directly:
JPH::RefConst<JPH::Shape> shape = new JPH::BoxShape(JPH::Vec3(1.0f, 0.5f, 2.0f));
```

Shapes are reference-counted (`RefConst<Shape>`). Assigning to a variable keeps
them alive.

### 2.3 Broadphase and Narrowphase

Collision detection has two stages:

**Broadphase** — fast, conservative. Determines which *pairs* of bodies are
*possibly* overlapping using cheap bounding volumes (axis-aligned bounding boxes,
BVH trees). It produces a candidate list, not definitive contacts.

**Narrowphase** — precise. Tests each candidate pair with the actual shapes
(GJK, EPA, SAT algorithms) to find exact contact points, penetration depth, and
contact normals.

Jolt exposes this split explicitly:
- `PhysicsSystem::GetBroadPhaseQuery()` — query the BVH without shape testing.
- `PhysicsSystem::GetNarrowPhaseQuery()` — query with full shape testing.

For game queries (raycasting to find what the player is looking at, ground
detection, etc.) you almost always want `GetNarrowPhaseQuery()`, because you
need to know the exact hit position and the body hit, not just "something is
nearby."

### 2.4 Collision Response

When the narrowphase finds overlapping bodies, Jolt computes **contact
constraints** — forces that push the bodies apart just enough to eliminate the
overlap. The solver iterates these constraints multiple times per step to find
a stable solution.

Key parameters that affect response:

- **Restitution** (0–1) — how bouncy the contact is. 0 = perfectly inelastic
  (objects stop), 1 = perfectly elastic (objects bounce without energy loss).
- **Friction** (0–∞) — how much lateral force is applied at the contact.
  Typical values: 0.1 (ice), 0.5 (concrete), 1.0+ (rubber).

Static bodies have *infinite mass* — they never move in response to forces.
Two static bodies never generate a contact constraint regardless of overlap.
This is important: **Jolt will not separate two static bodies even if they
completely intersect.**

### 2.5 Constraints

Constraints restrict degrees of freedom between two bodies. Common examples:

- **FixedConstraint** — welds two bodies together.
- **HingeConstraint** — allows rotation around one axis (door hinge).
- **SliderConstraint** — allows movement along one axis (piston).
- **PointConstraint** — bodies can rotate freely around a shared point (ball
  and socket).

This engine does not currently use constraints beyond what `CharacterVirtual`
handles internally.

### 2.6 The Fixed Timestep

Physics simulation is **inherently sensitive to the timestep size**. Variable
frame rates produce variable results — tunnelling (fast objects passing through
thin walls), unstable stacks, inconsistent feel. The standard solution is a
fixed physics tick rate, independent of rendering frame rate.

Our engine runs physics at **60 Hz** (a 16.67 ms step). The rendering loop runs
at whatever rate the GPU can sustain. The pipeline accumulates real elapsed time
and steps physics in fixed increments, discarding fractional time as accumulator
carry. See `src/pipeline.hpp` and `ARCH_STATE.md` §4 for the implementation.

Jolt's `PhysicsSystem::Update` takes an explicit `dt` parameter — it is the
engine's responsibility to call it with a consistent timestep, not Jolt's.

---

## 3. Jolt Architecture

### 3.1 Initialization

Jolt requires global initialization before any other call. The allocator must
be registered once per process lifetime:

```cpp
JPH::RegisterDefaultAllocator();          // use malloc/free
JPH::Factory::sInstance = new JPH::Factory();  // type registry
JPH::RegisterTypes();                     // register built-in shapes/constraints
```

In this engine, `PhysicsContext::InitJoltAllocator()` and the `PhysicsContext`
constructor perform these steps. See `src/physics_context.hpp`.

### 3.2 PhysicsSystem

`JPH::PhysicsSystem` is the root object. Everything else is accessed through it.

```cpp
physics_system->Init(
    max_bodies,              // maximum number of bodies (1024 in our setup)
    num_body_mutexes,        // 0 = auto
    max_body_pairs,          // max simultaneous broadphase contact pairs
    max_contact_constraints, // max contact constraints solved per step
    broad_phase_layer_interface,
    object_vs_broadphase_layer_filter,
    object_layer_pair_filter
);
```

The three filter objects are discussed in §4.

### 3.3 TempAllocator

Jolt allocates temporary data during each `Update` call (contact manifolds,
constraint islands, etc.). It uses a `TempAllocator` for this, which is a
fast stack allocator that avoids heap fragmentation.

```cpp
JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024); // 10 MB stack
```

The allocator must outlive all `PhysicsSystem::Update` calls. We store it in
`PhysicsContext` for this reason. Pass it to every `Update`:

```cpp
physics_system->Update(dt, num_steps, &temp_allocator, job_system);
```

Do not use the temp allocator for your own persistent data — its contents are
invalidated at the end of each Update call.

### 3.4 JobSystem

Jolt parallelises the physics step across threads via a `JobSystem`. In most
configurations, use `JobSystemThreadPool`:

```cpp
JPH::JobSystemThreadPool job_system(
    JPH::cMaxPhysicsJobs,
    JPH::cMaxPhysicsBarriers,
    std::thread::hardware_concurrency() - 1  // leave one core for the main thread
);
```

Pass it to every `Update`. Our `PhysicsContext` holds a single shared instance.

### 3.5 BodyInterface

`BodyInterface` is the main API for creating and manipulating bodies. Access it
from `PhysicsSystem`:

```cpp
JPH::BodyInterface& bi = physics_system->GetBodyInterface();
// or the read-only variant for thread-safe queries:
const JPH::BodyInterface& bi = physics_system->GetBodyInterfaceNoLock();
```

Important methods:

```cpp
// Create a body (does not add it to the simulation yet)
JPH::Body* body = bi.CreateBody(settings);

// Add to the simulation and optionally activate it
bi.AddBody(body->GetID(), JPH::EActivation::Activate);

// Remove from simulation (body still exists in memory)
bi.RemoveBody(body_id);

// Destroy the body object
bi.DestroyBody(body_id);

// Teleport (immediately set position, ignores physics)
bi.SetPosition(body_id, new_pos, JPH::EActivation::Activate);

// Read position and rotation
JPH::RVec3 pos;
JPH::Quat  rot;
bi.GetPositionAndRotation(body_id, pos, rot);

// Apply velocity
bi.SetLinearVelocity(body_id, velocity);
bi.AddLinearVelocity(body_id, delta_velocity);
```

### 3.6 Shapes and ShapeSettings

Prefer constructing shapes directly when you do not need serialisation:

```cpp
// Direct construction (no error checking needed for primitives)
JPH::RefConst<JPH::Shape> box    = new JPH::BoxShape(JPH::Vec3(0.5f, 1.0f, 0.5f));
JPH::RefConst<JPH::Shape> sphere = new JPH::SphereShape(0.4f);
JPH::RefConst<JPH::Shape> caps   = new JPH::CapsuleShape(0.9f, 0.4f);
//                                                        ^half_h  ^radius
```

For shapes that require validation (convex hull, mesh):

```cpp
JPH::ConvexHullShapeSettings hull_settings;
hull_settings.mPoints = { /* array of JPH::Vec3 */ };
auto result = hull_settings.Create();
if (result.HasError()) {
    // handle error: result.GetError() is a string
}
JPH::RefConst<JPH::Shape> hull = result.Get();
```

To offset a shape from its body's local origin (e.g. a capsule that stands
upright with its base at y=0 rather than centred):

```cpp
// Translate the capsule up so its base is at origin
JPH::RefConst<JPH::Shape> upright_cap = new JPH::RotatedTranslatedShape(
    JPH::Vec3(0, capsule_half_height + radius, 0),  // translation
    JPH::Quat::sIdentity(),                          // rotation
    new JPH::CapsuleShape(capsule_half_height, radius)
);
```

This is exactly what `CharacterMotorSystem` does for the player capsule.

---

## 4. Collision Layers — Jolt's Filtering System

This section is the most important for avoiding subtle bugs when writing queries.

### 4.1 Two Dimensions of Filtering

Jolt uses two distinct layer concepts:

| Concept | Purpose | Granularity |
|---------|---------|-------------|
| `ObjectLayer` | Fine-grained semantic category | Per-body, you define the values |
| `BroadPhaseLayer` | Which BVH tree bucket a body lives in | Coarse grouping of ObjectLayers |

Each body has exactly one `ObjectLayer`. The `BroadPhaseLayerInterface` maps
each ObjectLayer to a `BroadPhaseLayer`. Bodies in the same BroadPhaseLayer
share a BVH tree.

### 4.2 ObjectLayer

An `ObjectLayer` is just a `uint16_t`. You define the meaningful values:

```cpp
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;  // static geometry
    static constexpr JPH::ObjectLayer MOVING     = 1;  // dynamic + character
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};
```

Every body is assigned one when created via `BodyCreationSettings`. Static
geometry gets `NON_MOVING`; everything else gets `MOVING`.

### 4.3 BroadPhaseLayer

A `BroadPhaseLayer` determines the BVH bucket. We use two:

```cpp
namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
};
```

The mapping (in `BPLayerInterfaceImpl`) is one-to-one here:

```cpp
mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
mObjectToBroadPhase[Layers::MOVING]     = BroadPhaseLayers::MOVING;
```

Separating static and dynamic bodies into different BVH trees is a critical
optimisation: the static BVH is built once and never rebuilt, while the dynamic
BVH is updated every frame as bodies move. Querying only the static BVH (via
layer filtering) is much cheaper than querying everything.

### 4.4 The Three Filter Interfaces

Jolt requires three filter objects at `PhysicsSystem::Init` time and accepts
them as parameters to most query calls:

#### `BroadPhaseLayerInterface`

Maps each `ObjectLayer` to a `BroadPhaseLayer`. Required at init. Defines how
many BVH trees exist.

#### `ObjectVsBroadPhaseLayerFilter`

```cpp
bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const;
```

Used during the broadphase. For body-vs-body: given that body A has object layer
`inLayer1`, should it test against the BVH bucket `inLayer2`? For queries: given
that the query is labelled `inLayer1`, should it search bucket `inLayer2`?

#### `ObjectLayerPairFilter`

```cpp
bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const;
```

Used during the narrowphase. Given that body A has layer `inObject1` and body B
has layer `inObject2`, should a contact constraint be generated between them?
For queries: should the query return a hit on a body with layer `inObject2`?

### 4.5 Our Two-Layer Setup

```cpp
// ObjectVsBroadPhaseLayerFilterImpl
NON_MOVING -> only BroadPhaseLayers::MOVING   (static things don't collide with static)
MOVING     -> all BroadPhase layers           (dynamic things collide with everything)

// ObjectLayerPairFilterImpl
NON_MOVING -> only MOVING                     (static-static pairs: never generated)
MOVING     -> MOVING and NON_MOVING           (dynamic vs everything)
```

This is the canonical Jolt two-layer pattern. It:
- Prevents the solver generating pointless static-static contact constraints.
- Keeps the static BVH frozen (no rebuild cost).
- Is easy to reason about: "static things stay put, dynamic things interact with
  the world."

### 4.6 Why Pair Filters Break for Utility Queries

`DefaultBroadPhaseLayerFilter` and `DefaultObjectLayerFilter` are thin wrappers
that call your pair-filter objects. They are convenient for body-vs-body
collision but produce counter-intuitive results for utility queries.

**Example: ray against static geometry using `NON_MOVING` as the query layer.**

```cpp
JPH::DefaultBroadPhaseLayerFilter bp(filter, Layers::NON_MOVING);
// Internally: ShouldCollide(NON_MOVING, candidate_broadphase_layer)
//   -> NON_MOVING only collides with BroadPhaseLayers::MOVING
//   -> BroadPhaseLayers::NON_MOVING returns FALSE
//   -> The entire static BVH is skipped. Ray misses all static bodies.
```

The pair filter was designed to answer "should object A and object B collide in
the simulation?" The answer "NON_MOVING doesn't collide with NON_MOVING" is
correct for *body pairs* but wrong for a *query* whose intent is "find static
geometry."

This is a common Jolt gotcha, especially for developers coming from other
physics libraries (PhysX, Bullet) where layer filtering has a simpler linear
mask model.

### 4.7 The Correct Pattern for Utility Queries

**Option A — broadphase piggyback (current engine approach):**

Pass `Layers::MOVING` as the broadphase layer. Since MOVING collides with all
broadphase buckets, the ray searches both the static and dynamic BVH trees.
Then use a custom `ObjectLayerFilter` to narrow hits to only the desired type.

```cpp
// MOVING broadphase filter: searches all BVH buckets
JPH::DefaultBroadPhaseLayerFilter bp_filter(
    ctx.object_vs_broadphase_layer_filter, Layers::MOVING);

// Custom object filter: only accept static (NON_MOVING) bodies
struct StaticOnlyLayerFilter final : public JPH::ObjectLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer == Layers::NON_MOVING;
    }
};
StaticOnlyLayerFilter obj_filter;
```

**Option B — fully explicit filters (recommended for new queries):**

Write dedicated filter objects that express intent directly, independent of the
collision pair table:

```cpp
// Accept all broadphase buckets — no coupling to the pair filter
struct AllBroadPhaseFilter final : public JPH::BroadPhaseLayerFilter {
    bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; }
};

// Accept only static bodies
struct StaticOnlyLayerFilter final : public JPH::ObjectLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer == Layers::NON_MOVING;
    }
};

AllBroadPhaseFilter  bp_filter;
StaticOnlyLayerFilter obj_filter;
```

Option B is immune to future changes in the collision pair table. If a third
layer is ever added, Option A might silently stop searching the right buckets
while Option B continues working. Prefer Option B for any new query code.

---

## 5. Body Lifecycle

### 5.1 BodyCreationSettings

All body creation starts here:

```cpp
JPH::BodyCreationSettings settings(
    shape,                    // JPH::RefConst<JPH::Shape>
    position,                 // JPH::RVec3 (world position)
    rotation,                 // JPH::Quat
    motion_type,              // JPH::EMotionType::Static | Dynamic | Kinematic
    object_layer              // JPH::ObjectLayer (e.g. Layers::NON_MOVING)
);

// Physical material properties
settings.mRestitution = 0.0f;   // 0 = no bounce
settings.mFriction    = 0.5f;
settings.mIsSensor    = false;  // true = trigger volume (no response)

// Mass override for Dynamic bodies (optional)
settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
settings.mMassPropertiesOverride.mMass = 80.0f;
```

### 5.2 Creating and Adding Bodies

```cpp
JPH::BodyInterface& bi = ctx.physics_system->GetBodyInterface();

JPH::Body* body = bi.CreateBody(settings);
if (body == nullptr) {
    // Max body count reached — increase the limit in PhysicsSystem::Init
}

// Add to simulation. Activate wakes the body immediately.
bi.AddBody(body->GetID(), JPH::EActivation::Activate);

// Store the ID (not the pointer — the pointer is owned by Jolt)
JPH::BodyID id = body->GetID();
```

**Never store a raw `Body*` pointer.** The body may be moved in memory by Jolt's
internal management. Store and use the `BodyID` for all subsequent operations.

### 5.3 Motion Types

| `EMotionType` | Behaviour | Gravity | Responds to forces | Responds to collisions |
|---|---|---|---|---|
| `Static` | Never moves | No | No | No |
| `Dynamic` | Full simulation | Yes | Yes | Yes |
| `Kinematic` | Script-driven position | No | No (read by other bodies) | One-way |

**Static** — ground planes, walls, level geometry. Assigned `Layers::NON_MOVING`.
Two statics never interact. Never try to move a static body by setting velocity —
use `BodyInterface::SetPosition` (teleport) and call `NotifyShapeChanged` if needed.

**Dynamic** — props, physics objects. Full integration and collision response.

**Kinematic** — platforms, doors, elevators: moved by your code, but other bodies
respond to them as if they were Dynamic. Set position via `MoveKinematic` to get
correct contact velocities (prevents tunnelling through fast-moving kinematic
bodies).

### 5.4 Activation and Sleeping

Dynamic bodies that have not moved recently are put to sleep by Jolt to save
CPU. A sleeping body is excluded from the broadphase until it is woken.

```cpp
// Manually activate (wake up)
bi.ActivateBody(id);

// Check state
bool is_active = bi.IsActive(id);
```

Bodies are automatically woken when another active body touches them, or when
you call `SetLinearVelocity`, `AddForce`, etc.

### 5.5 Removing and Destroying Bodies

Always do this in two steps:

```cpp
bi.RemoveBody(id);   // detaches from simulation, body still allocated
bi.DestroyBody(id);  // frees memory
```

After `DestroyBody`, the `BodyID` is invalid. Our `on_remove<RigidBodyHandle>`
hook handles this automatically for ECS-managed bodies.

---

## 6. Querying the World

Queries interrogate the physics world without stepping the simulation. They are
safe to call between `PhysicsSystem::Update` calls (in the Logic phase).

### 6.1 BroadPhaseQuery vs NarrowPhaseQuery

```cpp
const JPH::BroadPhaseQuery& bpq = ctx.physics_system->GetBroadPhaseQuery();
const JPH::NarrowPhaseQuery& npq = ctx.physics_system->GetNarrowPhaseQuery();
```

- **BroadPhaseQuery** — cheap BVH intersection. Returns body IDs whose AABBs
  overlap the query. Does not test actual shapes. Useful for "is anything
  roughly near here?" without needing exact contacts.
- **NarrowPhaseQuery** — precise shape testing. Returns exact hit positions,
  fractions, normals, face indices. Required for raycasting, collision volumes,
  ground detection, etc.

Use `NarrowPhaseQuery` for almost everything. The extra cost of shape testing
is negligible for player-driven queries.

### 6.2 CastRay

Cast an infinite or finite ray and find the *first* hit:

```cpp
// Define the ray
JPH::RRayCast ray{
    JPH::RVec3(origin.x, origin.y, origin.z),  // start position (high precision)
    JPH::Vec3(dir.x * length, dir.y * length, dir.z * length)  // direction * max_length
};
// Note: mDirection is the full displacement vector, not a unit vector.
// The ray ends at mOrigin + mDirection.

JPH::RayCastResult result;

bool hit = npq.CastRay(ray, result, bp_filter, obj_filter, body_filter);
```

On hit, `result` contains:

```cpp
result.mBodyID;    // which body was hit
result.mFraction;  // in [0, 1] — how far along mDirection the hit occurred
result.mSubShapeID2;  // which sub-shape (for compound/mesh shapes)
```

Hit position:

```cpp
JPH::RVec3 hit_pos = ray.mOrigin + result.mFraction * ray.mDirection;
```

For *all* hits along the ray (not just the nearest), use `CastRay` with a
`CastRayCollector`:

```cpp
JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> collector;
npq.CastRay(ray, settings, collector, bp_filter, obj_filter, body_filter);
// collector.mHits is a vector of results
```

### 6.3 CollideShape

Tests a shape against the world and returns all overlapping bodies with contact
information:

```cpp
JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
npq.CollideShape(
    test_shape,             // JPH::Shape*
    JPH::Vec3::sReplicate(1.0f),  // scale
    JPH::RMat44::sTranslation(test_pos),  // shape transform
    JPH::CollideShapeSettings{},
    base_offset,
    collector,
    bp_filter, obj_filter, body_filter
);
// collector.mHits contains all contacts
```

Useful for overlap testing (e.g. "is there any body inside this box?") and for
computing push-out vectors.

### 6.4 CastShape

Sweeps a shape through the world along a vector and finds where it first
contacts another body. Useful for character step-up detection, swept collision,
projectile simulation with thick bullets.

```cpp
JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
JPH::ShapeCastSettings cast_settings;
npq.CastShape(
    JPH::RShapeCast{shape, scale, start_transform, displacement},
    cast_settings,
    base_offset,
    collector,
    bp_filter, obj_filter, body_filter, shape_filter
);
```

### 6.5 Writing Correct Filters for Queries

Every query takes three filter parameters:

| Parameter | Type | Filters at stage |
|---|---|---|
| `inBroadPhaseLayerFilter` | `BroadPhaseLayerFilter&` | BVH traversal |
| `inObjectLayerFilter` | `ObjectLayerFilter&` | Post-broadphase, pre-narrow |
| `inBodyFilter` | `BodyFilter&` | Per-body, can exclude specific IDs |

The pattern for common cases (see §4.7 for the full explanation):

```cpp
// Query hits everything
struct AllBPFilter  : public JPH::BroadPhaseLayerFilter { bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; } };
struct AllObjFilter : public JPH::ObjectLayerFilter     { bool ShouldCollide(JPH::ObjectLayer)     const override { return true; } };

// Query hits only static bodies
struct AllBPFilter    : public JPH::BroadPhaseLayerFilter { bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; } };
struct StaticObjFilter: public JPH::ObjectLayerFilter     { bool ShouldCollide(JPH::ObjectLayer inL) const override { return inL == Layers::NON_MOVING; } };

// Exclude a specific body (e.g. the player's own body)
struct ExcludeBodyFilter : public JPH::BodyFilter {
    JPH::BodyID mExclude;
    ExcludeBodyFilter(JPH::BodyID id) : mExclude(id) {}
    bool ShouldCollide(const JPH::BodyID& id)  const override { return id != mExclude; }
    bool ShouldCollideLocked(const JPH::Body&) const override { return true; }
};
```

The default `JPH::BodyFilter{}` accepts all bodies.

---

## 7. CharacterVirtual

### 7.1 Why Not a Rigid Body?

A naive approach to a player character is a dynamic capsule rigid body. This has
several problems:

- **Stairs and steps** — a rigid capsule will bounce or get stuck on geometry
  edges rather than stepping up smoothly.
- **Ground adherence** — a dynamic body slides off slopes and cannot easily
  maintain contact while moving horizontally.
- **Gravity coupling** — separating the feel of player gravity (fast-responsive)
  from simulation gravity is awkward.
- **Rotation** — dynamic bodies can be torqued by contacts, causing the player
  to tilt.

`CharacterVirtual` solves these by performing its own per-step shape casts,
implementing step-up/step-down logic, slope detection, and explicit velocity
integration — all without adding a body to the Jolt simulation. It is a client
of the narrowphase, not a simulated object.

### 7.2 CharacterVirtualSettings

```cpp
JPH::CharacterVirtualSettings settings;
settings.mMass             = 70.0f;
settings.mMaxSlopeAngle    = JPH::DegreesToRadians(45.0f);
settings.mShape            = upright_capsule_shape;
settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cfg.radius);
```

`mMaxSlopeAngle` — slopes steeper than this are treated as walls; the character
slides along them rather than climbing.

`mSupportingVolume` — a plane that defines the character's "feet." Ground contact
is detected at the intersection of this plane with the character's shape. See §7.3.

```cpp
auto character = std::make_shared<JPH::CharacterVirtual>(
    &settings,
    initial_position,
    JPH::Quat::sIdentity(),
    physics_system
);
```

### 7.3 mSupportingVolume and Ground Detection

`mSupportingVolume` is a `JPH::Plane` defined as `(normal, d)` where the plane
equation is `dot(normal, point) + d = 0`. For an upright character:

```cpp
JPH::Plane(JPH::Vec3::sAxisY(), -cfg.radius)
// normal = (0,1,0), d = -radius
// plane: y - radius = 0  ->  y = radius below character origin
```

If `cfg.radius = 0.4f`, the feet plane is at `character.GetPosition().y - 0.4`.

This means when you read the character's world position, its feet are `radius`
below that Y coordinate. The `CharacterState` and `PlatformBuilderSystem` both
use `k_char_radius = 0.4f` for this offset.

Ground state is read after `ExtendedUpdate`:

```cpp
auto ground_state = character->GetGroundState();
bool on_ground = (ground_state == JPH::CharacterVirtual::EGroundState::OnGround);
```

States: `OnGround`, `OnSteepGround`, `NotSupported`, `InAir`.

### 7.4 ExtendedUpdate

The primary step function:

```cpp
JPH::CharacterVirtual::ExtendedUpdateSettings ext_settings;
// defaults are usually fine

character->ExtendedUpdate(
    dt,
    gravity_vector,       // e.g. JPH::Vec3(0, -9.81f, 0)
    ext_settings,
    bp_filter,
    obj_filter,
    body_filter,
    shape_filter,
    *temp_allocator
);
```

`ExtendedUpdate` performs:
1. Velocity integration (applies gravity internally).
2. Shape cast along the velocity vector to find contacts.
3. Step-up detection: tries to step over small obstacles.
4. Step-down detection: tries to maintain ground contact on slopes.
5. Constraint solving for contact normals.
6. Position update.

**This must run in the Logic phase, before `PhysicsSystem::Update`**, because it
reads the current state of static bodies (which are authoritative at all times)
and dynamic bodies (whose positions come from the previous tick). Running it
after the physics step would use stale dynamic body positions.

### 7.5 Velocity and Rotation

```cpp
// Set absolute velocity (call before ExtendedUpdate)
character->SetLinearVelocity(new_velocity);

// Read velocity (after ExtendedUpdate, reflects contact resolution)
JPH::Vec3 vel = character->GetLinearVelocity();

// Rotate the character
character->SetRotation(new_rotation);

// Read position (after ExtendedUpdate)
JPH::RVec3 pos = character->GetPosition();
```

`CharacterVirtual` does not have angular velocity — it is a position/orientation
object, not a rigid body. Rotation is set directly.

---

## 8. Thread Safety

Jolt parallelises internally during `PhysicsSystem::Update` via the JobSystem.
Outside of `Update`, the rules are:

| Operation | Thread safe? |
|---|---|
| `GetBodyInterfaceNoLock()` reads | Yes (no body structural changes in flight) |
| `GetBodyInterface()` reads | Yes (uses internal locks) |
| `GetBodyInterface()` writes during `Update` | No |
| `GetNarrowPhaseQuery()` | Yes (read-only access to shapes/BVH) |
| `GetBroadPhaseQuery()` | Yes |
| `CreateBody` / `AddBody` / `DestroyBody` | Safe between Updates |
| `CharacterVirtual::ExtendedUpdate` | Safe (does not modify simulation state) |

In practice: do everything in the Logic phase (before the physics tick) or after
it. Never call `BodyInterface` write methods from a parallel task that runs
concurrently with `PhysicsSystem::Update`.

Our pipeline enforces this implicitly — Logic runs, then Physics runs, then
Render runs, all serially.

---

## 9. How We Use Jolt in This Engine

### 9.1 PhysicsContext Resource

All Jolt objects are owned by a `PhysicsContext` struct (`src/physics_context.hpp`)
stored as an ECS resource:

```cpp
// PhysicsModule installs it:
world.set_resource(std::make_shared<PhysicsContext>());

// Any system can access it:
auto* ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
if (!ctx_ptr || !*ctx_ptr) return;  // not installed (headless test target)
auto& ctx = **ctx_ptr;

JPH::BodyInterface& bi = ctx.GetBodyInterface();
const JPH::NarrowPhaseQuery& npq = ctx.physics_system->GetNarrowPhaseQuery();
```

`PhysicsContext` owns:
- `temp_allocator` — `TempAllocatorImpl*`
- `job_system` — `JobSystemThreadPool*`
- `physics_system` — `PhysicsSystem*`
- `broad_phase_layer_interface` — `BPLayerInterfaceImpl`
- `object_vs_broadphase_layer_filter` — `ObjectVsBroadPhaseLayerFilterImpl`
- `object_layer_pair_filter` — `ObjectLayerPairFilterImpl`

The `shared_ptr` indirection allows the resource to be null in the headless
test target, which does not initialise Jolt at all. Every system that touches
Jolt must guard with the `!ctx_ptr || !*ctx_ptr` check.

### 9.2 MathBridge

Jolt and the ECS use different math types. `src/physics_handles.hpp` provides
inline conversion functions:

```cpp
namespace MathBridge {
    JPH::Vec3  ToJolt(const ecs::Vec3& v);
    JPH::Quat  ToJolt(const ecs::Quat& q);
    ecs::Vec3  FromJolt(const JPH::Vec3& v);
    ecs::Quat  FromJolt(const JPH::Quat& q);
    ecs::Vec3  FromJolt(const JPH::RVec3& v);  // double-precision positions
}
```

Always go through `MathBridge` — never access `.x`/`.y`/`.z` on Jolt types
directly (the accessor functions `GetX()`, `GetY()`, `GetZ()` exist for a
reason: `Vec3` may use SIMD storage where direct member access is wrong).

`JPH::RVec3` is the double-precision world position type used by Jolt for
large-world support. Body positions are `RVec3`. When converting to ECS `Vec3`
(float), precision loss is expected and acceptable within a normal game world
size. If the world grows very large (> a few hundred metres), floating-point
jitter may become visible and a camera-relative origin system would be needed.

### 9.3 Body Creation via on_add Hooks

Bodies are not created directly in system Update loops. Instead, the
`PhysicsSystem::Register` function installs an `on_add<RigidBodyConfig>` hook:

```cpp
// src/systems/physics.cpp
world.on_add<RigidBodyConfig>([&](World& w, Entity e, RigidBodyConfig& cfg) {
    // Read collider shape from sibling components
    JPH::RefConst<JPH::Shape> shape;
    if (auto* box = w.try_get<BoxCollider>(e)) {
        shape = new JPH::BoxShape(MathBridge::ToJolt(box->half_extents));
    } else if (auto* sphere = w.try_get<SphereCollider>(e)) {
        shape = new JPH::SphereShape(sphere->radius);
    }

    // Read initial position from transform
    JPH::Vec3 pos = JPH::Vec3::sZero();
    if (auto* lt = w.try_get<LocalTransform>(e)) {
        pos = MathBridge::ToJolt(lt->position);
    }

    // Determine motion type and layer
    JPH::EMotionType motion = (cfg.type == BodyType::Static)
        ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
    JPH::ObjectLayer layer = (cfg.type == BodyType::Static)
        ? Layers::NON_MOVING : Layers::MOVING;

    JPH::BodyCreationSettings settings(shape, pos, rot, motion, layer);
    settings.mRestitution = cfg.restitution;
    settings.mFriction    = cfg.friction;
    settings.mIsSensor    = cfg.sensor;

    JPH::Body* body = bi.CreateBody(settings);
    bi.AddBody(body->GetID(), JPH::EActivation::Activate);

    // Attach the Jolt ID back to the ECS entity
    w.add(e, RigidBodyHandle{body->GetID()});
});
```

The hook fires automatically whenever `RigidBodyConfig` is added to any entity
— at scene load, or via `world.deferred().create_with(...)`. You never need to
call body creation code manually.

A matching `on_remove<RigidBodyHandle>` hook removes and destroys the Jolt body
when the entity is destroyed.

### 9.4 The Physics Phase and Fixed Step

`PhysicsModule::install` wires the physics tick into the Pipeline's Physics
phase:

```cpp
pipeline.add_physics([](ecs::World& w, float dt) {
    PhysicsSystem::Update(w, dt);    // steps Jolt
    ecs::propagate_transforms(w);   // ECS hierarchy update
});
```

`PhysicsSystem::Update` calls:

```cpp
ctx.physics_system->Update(dt, 1, ctx.temp_allocator, ctx.job_system);
```

The `1` is `num_collision_steps` per Update call — the number of sub-steps Jolt
uses internally for stability. Higher values improve tunnelling resistance for
fast objects at the cost of CPU time. 1 is correct for our 60 Hz fixed step.

### 9.5 Transform Synchronisation

After each physics tick, dynamic bodies' positions are written back to ECS
components:

```cpp
// src/systems/physics.cpp — PhysicsSystem::Update
world.each<RigidBodyHandle, WorldTransform, RigidBodyConfig>(
    [&](Entity e, RigidBodyHandle& h, WorldTransform& wt, RigidBodyConfig& cfg) {
        if (cfg.type == BodyType::Dynamic) {
            JPH::RVec3 pos;
            JPH::Quat  rot;
            bi.GetPositionAndRotation(h.id, pos, rot);
            wt.matrix = mat4_compose(
                MathBridge::FromJolt(pos),
                MathBridge::FromJolt(rot),
                {1, 1, 1}
            );
        }
    });
```

Static bodies never move, so their `WorldTransform` is set once at creation and
never updated. Kinematic bodies would need to be moved via `MoveKinematic` and
then synced similarly to Dynamic if we used them.

The character controller follows a different path — `CharacterMotorSystem`
writes `LocalTransform` directly after `ExtendedUpdate` in the Logic phase,
before `propagate_transforms` runs in the Physics phase.

### 9.6 CharacterMotorSystem

`src/systems/character_motor.cpp` shows the complete Jolt character loop:

```cpp
void CharacterMotorSystem::Update(World& world, float dt) {
    auto* ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
    if (!ctx_ptr || !*ctx_ptr) return;
    auto& ctx = **ctx_ptr;

    world.each<CharacterHandle, CharacterIntent, CharacterState, WorldTransform>(
        [&](Entity e, CharacterHandle& h, CharacterIntent& intent,
            CharacterState& state, WorldTransform& wt) {

            auto* ch = h.character.get();

            // 1. Compute desired velocity from intent
            JPH::Vec3 move_dir = MathBridge::ToJolt(intent.move_dir);
            JPH::Vec3 target_vel = move_dir * 10.0f;
            // ... horizontal/vertical blending ...
            ch->SetLinearVelocity(new_vel);

            // 2. Step the character through the world
            JPH::DefaultBroadPhaseLayerFilter bp_filter(
                ctx.object_vs_broadphase_layer_filter, Layers::MOVING);
            JPH::DefaultObjectLayerFilter obj_filter(
                ctx.object_layer_pair_filter, Layers::MOVING);
            JPH::BodyFilter  body_filter;
            JPH::ShapeFilter shape_filter;
            JPH::CharacterVirtual::ExtendedUpdateSettings ext_settings;

            ch->ExtendedUpdate(dt, {0, -9.81f, 0}, ext_settings,
                               bp_filter, obj_filter, body_filter, shape_filter,
                               *ctx.temp_allocator);

            // 3. Write position back to ECS
            if (auto* lt = world.try_get<LocalTransform>(e)) {
                lt->position = MathBridge::FromJolt(ch->GetPosition());
                lt->rotation = MathBridge::FromJolt(ch->GetRotation());
                wt.matrix    = mat4_compose(lt->position, lt->rotation, lt->scale);
            }
        });
}
```

Note that `CharacterMotorSystem` uses `Layers::MOVING` for its `ExtendedUpdate`
filters — this is correct because `ExtendedUpdate` represents the character
moving through the world, and MOVING collides with everything including static
geometry. This is the simulation use-case that the pair-filter table is designed
for, so the standard `DefaultBroadPhaseLayerFilter` / `DefaultObjectLayerFilter`
pattern works correctly here.

### 9.7 PlatformBuilderSystem — Raycast Pattern

`src/systems/builder.cpp` demonstrates the utility query pattern from §4.7
(Option A):

```cpp
// Detect static geometry within the platform spawn volume
struct StaticOnlyLayerFilter final : public JPH::ObjectLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer == Layers::NON_MOVING;
    }
};

float feet_y  = player_pos.y - k_char_radius;   // 0.4 below character origin
float spawn_y = feet_y - k_platform_half_h;      // default: top at feet

JPH::RRayCast ray{
    JPH::RVec3(player_pos.x, feet_y, player_pos.z),
    JPH::Vec3(0.f, -(size.y + 0.01f), 0.f)  // down by 0.51m
};
JPH::RayCastResult result;
JPH::DefaultBroadPhaseLayerFilter bp_filter(
    ctx.object_vs_broadphase_layer_filter, Layers::MOVING);  // sees all BVH buckets
StaticOnlyLayerFilter obj_filter;
JPH::BodyFilter body_filter;

if (ctx.physics_system->GetNarrowPhaseQuery()
        .CastRay(ray, result, bp_filter, obj_filter, body_filter)) {
    float surface_top   = feet_y + result.mFraction * ray.mDirection.GetY();
    float surface_based = surface_top + k_platform_half_h;
    spawn_y = std::max(spawn_y, surface_based);
}
```

The ray travels downward from the character's feet by the full platform height.
If it hits a static surface, the platform is raised so its top face is flush
with the detected surface. `std::max` ensures the mid-air case (ray misses) uses
the feet-level default.

---

## 10. Recipes

The following snippets are complete, copy-pasteable patterns for common Jolt
tasks in this engine.

### 10.1 Static Box Collider

Via the ECS authoring path (preferred — hooks handle the rest):

```cpp
world.deferred().create_with(
    ecs::LocalTransform{position, rotation, scale},
    ecs::WorldTransform{},
    BoxCollider{{half_x, half_y, half_z}},
    RigidBodyConfig{BodyType::Static},
    WorldTag{}
);
```

Directly via Jolt:

```cpp
auto& ctx = **world.try_resource<std::shared_ptr<PhysicsContext>>();
JPH::BodyInterface& bi = ctx.GetBodyInterface();

JPH::RefConst<JPH::Shape> shape = new JPH::BoxShape(JPH::Vec3(hx, hy, hz));
JPH::BodyCreationSettings settings(
    shape,
    JPH::RVec3(px, py, pz),
    JPH::Quat::sIdentity(),
    JPH::EMotionType::Static,
    Layers::NON_MOVING
);
JPH::BodyID id = bi.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
```

### 10.2 Dynamic Rigid Body

```cpp
world.deferred().create_with(
    ecs::LocalTransform{position, rotation, scale},
    ecs::WorldTransform{},
    SphereCollider{0.5f},
    RigidBodyConfig{BodyType::Dynamic, /*mass=*/5.0f, /*friction=*/0.3f, /*restitution=*/0.4f},
    WorldTag{}
);
```

### 10.3 Character Controller

```cpp
world.deferred().create_with(
    ecs::LocalTransform{spawn_pos, identity_rot, {1,1,1}},
    ecs::WorldTransform{},
    CharacterControllerConfig{
        .height          = 1.8f,
        .radius          = 0.4f,
        .mass            = 70.0f,
        .max_slope_angle = 45.0f
    },
    CharacterIntent{},
    CharacterState{},
    PlayerInput{},
    PlayerState{},
    PlayerTag{}
);
```

`CharacterMotorSystem::Register` installs the `on_add<CharacterControllerConfig>`
hook that creates the `CharacterVirtual` and attaches a `CharacterHandle`.

### 10.4 Raycast Against Static Geometry Only

```cpp
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

struct StaticOnlyLayerFilter final : public JPH::ObjectLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
        return inLayer == Layers::NON_MOVING;
    }
};

auto* ctx_ptr = world.try_resource<std::shared_ptr<PhysicsContext>>();
if (!ctx_ptr || !*ctx_ptr) return;
auto& ctx = **ctx_ptr;

JPH::RRayCast ray{
    JPH::RVec3(origin.x, origin.y, origin.z),
    JPH::Vec3(dir.x * max_dist, dir.y * max_dist, dir.z * max_dist)
};
JPH::RayCastResult result;
JPH::DefaultBroadPhaseLayerFilter bp_filter(
    ctx.object_vs_broadphase_layer_filter, Layers::MOVING);
StaticOnlyLayerFilter obj_filter;
JPH::BodyFilter body_filter;

if (ctx.physics_system->GetNarrowPhaseQuery()
        .CastRay(ray, result, bp_filter, obj_filter, body_filter)) {
    JPH::RVec3 hit_pos = ray.mOrigin + result.mFraction * ray.mDirection;
    // use hit_pos, result.mBodyID
}
```

### 10.5 Raycast Against All Geometry

```cpp
struct AllBroadPhaseFilter final : public JPH::BroadPhaseLayerFilter {
    bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; }
};
struct AllObjectFilter final : public JPH::ObjectLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer) const override { return true; }
};

AllBroadPhaseFilter bp_filter;
AllObjectFilter     obj_filter;
JPH::BodyFilter     body_filter;

JPH::RayCastResult result;
if (ctx.physics_system->GetNarrowPhaseQuery()
        .CastRay(ray, result, bp_filter, obj_filter, body_filter)) {
    // result.mBodyID — look up which entity this belongs to if needed
}
```

### 10.6 Sensor / Trigger Volume

A sensor generates no collision response but reports overlaps via contact
listeners. Set `mIsSensor = true` in `RigidBodyConfig`:

```cpp
RigidBodyConfig cfg;
cfg.type   = BodyType::Static;   // or Dynamic/Kinematic
cfg.sensor = true;
```

To receive overlap events, implement `JPH::ContactListener` and register it:

```cpp
physics_system->SetContactListener(&my_listener);
```

The listener's `OnContactAdded` / `OnContactRemoved` callbacks are called from
within `PhysicsSystem::Update` on a job thread — keep them lock-free and fast.
Writing into a lock-free event queue and processing in the Logic phase the
following frame is the recommended pattern.

---

## 11. Common Pitfalls

### Using `NON_MOVING` as the broadphase filter layer for utility queries

**Symptom:** Raycast returns no hits even though static geometry clearly exists.

**Cause:** `ObjectVsBroadPhaseLayerFilter::ShouldCollide(NON_MOVING, NON_MOVING)`
returns false in the canonical two-layer setup. The broadphase skips the static
BVH entirely.

**Fix:** Use `Layers::MOVING` for the broadphase filter (or write a custom
`AllBroadPhaseFilter`), and restrict to static bodies with a custom
`ObjectLayerFilter`. See §4.7.

### Storing a raw `Body*` pointer

**Symptom:** Crash or corrupt data on the next physics step.

**Cause:** Jolt may reallocate body storage. The `Body*` you get from
`CreateBody` is only valid until the next structural change to the body list.

**Fix:** Store `JPH::BodyID`. Look up body properties via `BodyInterface` as
needed.

### Two static bodies overlapping

**Symptom:** Visible geometry interpenetration with no correction.

**Cause:** Jolt never generates contact constraints between two Static bodies.
Static-vs-static overlap is permanent.

**Fix:** Compute the correct spawn position *before* creating the body (using a
raycast or shape query). The platform builder fix in RFC-0014 demonstrates this.

### Querying during `PhysicsSystem::Update`

**Symptom:** Crash or incorrect results from query calls.

**Cause:** The BVH is modified during `Update` as bodies move. Querying
concurrently from outside the job system is unsafe.

**Fix:** All queries must run either in the Logic phase (before `Update`) or in
the post-physics part of the Physics phase (after `Update` returns). Our pipeline
enforces this: systems run serially in phase order.

### Applying forces to a static body

**Symptom:** Body does not move; no error reported.

**Cause:** Static bodies have infinite mass. `AddForce` and `SetLinearVelocity`
are no-ops.

**Fix:** Change the body's motion type to Dynamic or Kinematic, or use
`BodyInterface::SetPosition` to teleport (which does not honour physics).

### `RVec3` vs `Vec3` mismatch

**Symptom:** Compiler error or silent precision loss.

**Cause:** Jolt uses double-precision `RVec3` for world positions and
single-precision `Vec3` for directions/velocities. They are not interchangeable.

**Fix:** Use `RVec3` for anything that is a world position (ray origins, body
positions). Use `Vec3` for directions, velocities, extents, and offsets.
`MathBridge::FromJolt(JPH::RVec3)` converts with an explicit float cast.

### Forgetting to `RemoveBody` before `DestroyBody`

**Symptom:** Assert or crash inside Jolt.

**Cause:** `DestroyBody` expects the body to have been removed from the
simulation first.

**Fix:** Always call `bi.RemoveBody(id)` then `bi.DestroyBody(id)`. Our
`on_remove<RigidBodyHandle>` hook does this automatically for ECS-managed bodies.

### Shape goes out of scope before the body is created

**Symptom:** Body has no collision or crash during shape access.

**Cause:** `JPH::RefConst<Shape>` uses reference counting. If the local variable
holding the shape goes out of scope before `CreateBody` increments the refcount,
the shape is freed.

**Fix:** Keep the shape in scope until after `CreateBody` returns, or assign it
to `BodyCreationSettings.mShape` before the local goes out of scope. The shape
is safe to release after that — `BodyCreationSettings` (and ultimately the body)
holds its own reference.
