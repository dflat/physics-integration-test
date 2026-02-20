#include "scene.hpp"
#include "components.hpp"
#include <ecs/modules/transform.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ecs::Vec3 parse_vec3(const json& j) {
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

static ecs::Quat parse_quat(const json& j) {
    // stored as [x, y, z, w]
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}

static Color4 parse_color4(const json& j) {
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
}

static ShapeType parse_shape(const std::string& s) {
    if (s == "Box")     return ShapeType::Box;
    if (s == "Sphere")  return ShapeType::Sphere;
    if (s == "Capsule") return ShapeType::Capsule;
    throw std::runtime_error("SceneLoader: unknown shape '" + s + "'");
}

static BodyType parse_body_type(const std::string& s) {
    if (s == "Static")    return BodyType::Static;
    if (s == "Dynamic")   return BodyType::Dynamic;
    if (s == "Kinematic") return BodyType::Kinematic;
    throw std::runtime_error("SceneLoader: unknown body type '" + s + "'");
}

// ---------------------------------------------------------------------------
// Entity spawning
// ---------------------------------------------------------------------------

static void spawn_entity(ecs::World& world, const json& e) {
    auto ent = world.create();

    // 1. LocalTransform + WorldTransform (must precede physics hooks)
    if (e.contains("transform")) {
        const auto& t = e["transform"];
        ecs::Vec3 pos = t.contains("position") ? parse_vec3(t["position"]) : ecs::Vec3{0,0,0};
        ecs::Quat rot = t.contains("rotation") ? parse_quat(t["rotation"]) : ecs::Quat{0,0,0,1};
        ecs::Vec3 scl = t.contains("scale")    ? parse_vec3(t["scale"])    : ecs::Vec3{1,1,1};
        world.add(ent, ecs::LocalTransform{pos, rot, scl});
        world.add(ent, ecs::WorldTransform{});
    }

    // 2. Colliders (must precede RigidBodyConfig so PhysicsSystem can read them)
    if (e.contains("box_collider")) {
        world.add(ent, BoxCollider{parse_vec3(e["box_collider"]["half_extents"])});
    }
    if (e.contains("sphere_collider")) {
        world.add(ent, SphereCollider{e["sphere_collider"]["radius"].get<float>()});
    }

    // 3. Visual representation
    if (e.contains("mesh")) {
        const auto& m = e["mesh"];
        ShapeType shape        = parse_shape(m.value("shape", std::string("Box")));
        Color4    color        = m.contains("color")        ? parse_color4(m["color"])        : Colors::White;
        ecs::Vec3 scale_offset = m.contains("scale_offset") ? parse_vec3(m["scale_offset"])   : ecs::Vec3{1,1,1};
        world.add(ent, MeshRenderer{shape, color, scale_offset});
    }

    // 4. Physics / character (triggers on_add lifecycle hooks — added last so
    //    sibling components are already present when the hook fires)
    if (e.contains("rigid_body")) {
        const auto& rb = e["rigid_body"];
        RigidBodyConfig cfg;
        cfg.type        = parse_body_type(rb.value("type", std::string("Dynamic")));
        cfg.mass        = rb.value("mass",        1.0f);
        cfg.friction    = rb.value("friction",    0.5f);
        cfg.restitution = rb.value("restitution", 0.0f);
        cfg.sensor      = rb.value("sensor",      false);
        world.add(ent, std::move(cfg));
    }
    if (e.contains("character")) {
        const auto& ch = e["character"];
        CharacterControllerConfig cfg;
        cfg.height          = ch.value("height",          1.8f);
        cfg.radius          = ch.value("radius",          0.4f);
        cfg.mass            = ch.value("mass",            70.0f);
        cfg.max_slope_angle = ch.value("max_slope_angle", 45.0f);
        world.add(ent, std::move(cfg));
    }

    // 5. Tags and player-specific components
    if (e.contains("tags")) {
        for (const auto& tag : e["tags"]) {
            const std::string t = tag.get<std::string>();
            if (t == "World")  world.add(ent, WorldTag{});
            if (t == "Player") {
                world.add(ent, PlayerTag{});
                world.add(ent, PlayerInput{});
                world.add(ent, PlayerState{});
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool SceneLoader::load_from_string(ecs::World& world, const std::string& json_str) {
    try {
        json scene = json::parse(json_str);
        for (const auto& entity_json : scene.at("entities")) {
            spawn_entity(world, entity_json);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool SceneLoader::load(ecs::World& world, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    const std::string content(std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>{});
    return load_from_string(world, content);
}

void SceneLoader::unload(ecs::World& world) {
    std::vector<ecs::Entity> to_destroy;
    world.each<WorldTag>([&](ecs::Entity e, WorldTag&) { to_destroy.push_back(e); });
    for (auto e : to_destroy) world.destroy(e);
    world.deferred().flush(world);
}
