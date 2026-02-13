#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <memory>
#include <iostream>
#include <vector>

// Layer definitions
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
};

// ---------------------------------------------------------------------------
// Jolt Boilerplate Implementation
// ---------------------------------------------------------------------------

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        // Create a mapping table from object layer to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true; // Collides with everything
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING; // Non moving only collides with moving
            case Layers::MOVING:
                return true; // Moving collides with everything
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

// ---------------------------------------------------------------------------
// Physics Context Resource
// ---------------------------------------------------------------------------

class PhysicsContext {
public:
    JPH::TempAllocatorImpl* temp_allocator = nullptr;
    JPH::JobSystemThreadPool* job_system = nullptr;
    JPH::PhysicsSystem* physics_system = nullptr;

    // Layer interfaces
    BPLayerInterfaceImpl broad_phase_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
    ObjectLayerPairFilterImpl object_layer_pair_filter;

    // Allocator for Jolt (Singleton)
    static void InitJoltAllocator() {
        JPH::RegisterDefaultAllocator();
    }

    PhysicsContext() {
        // Create Factory
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        // Init Physics System
        temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
        job_system = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
        
        physics_system = new JPH::PhysicsSystem();
        physics_system->Init(1024, 0, 1024, 1024, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_layer_pair_filter);

        std::cout << "Jolt Physics Initialized." << std::endl;
    }

    ~PhysicsContext() {
        if (physics_system) delete physics_system;
        if (job_system) delete job_system;
        if (temp_allocator) delete temp_allocator;
        if (JPH::Factory::sInstance) {
             delete JPH::Factory::sInstance;
             JPH::Factory::sInstance = nullptr;
        }
    }

    // Helper to optimize body creation
    JPH::BodyInterface& GetBodyInterface() { return physics_system->GetBodyInterface(); }
    const JPH::BodyInterface& GetBodyInterface() const { return physics_system->GetBodyInterface(); }
};
