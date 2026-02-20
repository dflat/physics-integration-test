# Template / Molecule Builder Mode

## Concept Overview

The goal is to transition from simple "atomic" building (placing single platforms) to a more expressive "molecule" building system. This involves a dedicated creative space where players can assemble complex structures from basic pieces and save them as reusable templates.

### The "Molecule" Metaphor
- **Atom:** A single primitive structure (e.g., a 1x1 platform, a beam, a pillar).
- **Molecule:** A group of atoms arranged in a specific spatial configuration, saved as a single logical unit.
- **Template:** The blueprint of a molecule that can be "stamped" into the game world.

---

## Core Mechanics

### 1. The Builder Mode (Local Space)
When entering Builder Mode, the main game simulation is suspended (or the player is transported to a void).
- **Local Coordinate System:** The builder operates around a `(0,0,0)` origin, which acts as the anchor point for the template.
- **Atomic Palette:** Access to basic geometric primitives (Box, Sphere, etc. from `MeshRenderer` / `BoxCollider`).
- **Manipulation:** standard move/rotate/scale tools for individual atoms within the molecule.

### 2. Saving & Templating
Once satisfied with an arrangement, the player "saves" the structure.
- **Serialization:** The collection of entities in the local space is serialized into a `Prefab`.
- **Inventory Integration:** The saved `Prefab` is added to the player's "Template Library."
- **Icon Generation:** Ideally, a small thumbnail is rendered from the local space camera for the UI.

### 3. World Stamping
Back in the game world, the player can select a template from their inventory.
- **Preview:** A "ghost" version of the molecule follows the player's cursor/crosshair.
- **Placement:** Clicking "stamps" the entire molecule into the world as a hierarchy of entities.
- **Physics:** The molecule can either be a single compound rigid body or a collection of joined bodies (depending on the desired destruction/stability mechanics).

---

## Technical Implementation Thoughts

### ECS Integration
- **Hierarchy:** Use the `ecs::modules::Hierarchy` system to parent all atoms to a single root "Molecule" entity.
- **Prefabs:** Leverage the existing `ecs::Prefab` system. A template is essentially a `std::vector<ecs::Prefab>` or a specialized `MoleculePrefab` that handles hierarchy instantiation.
- **Transforms:** 
    - Atoms have `LocalTransform` relative to the Molecule root.
    - Stamping sets the Molecule root's `WorldTransform`.

### Workflow Example
1. **Enter Mode:** Press `B`.
2. **Build:** Place a platform, rotate it 45 degrees, place another on top.
3. **Save:** Name it "Ramp_Double".
4. **Exit:** Return to the game.
5. **Use:** Select "Ramp_Double" from the quickbar and place 5 of them to reach a high ledge.

### Potential Challenges
- **Physics Complexity:** Compound shapes in Jolt need to be generated for molecules to ensure efficient collision.
- **Connectivity:** Should molecules be "fused" (single body) or "linked" (multiple bodies with joints)?
- **UI:** Needs a clean way to manage a library of custom-built templates.

---

## Future Extensions
- **Nesting:** Using molecules as atoms in even larger molecules.
- **Logic:** Adding functional atoms (e.g., a "Motor" atom or a "Sensor" atom) to create mechanical molecules.
- **Sharing:** Exporting template files for other players to use.
