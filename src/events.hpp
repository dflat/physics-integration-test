#pragma once
#include <ecs/ecs.hpp>
#include <functional>
#include <vector>

// ---------------------------------------------------------------------------
// Events<T> — typed, frame-scoped event queue
//
// Stored as a World resource. Systems emit via send() and consume via read().
// EventRegistry::flush_all() clears all queues at the start of each frame.
// ---------------------------------------------------------------------------

template<typename T>
struct Events {
    void send(T event)                     { buffer_.push_back(std::move(event)); }
    const std::vector<T>& read()   const  { return buffer_; }
    bool                  empty()  const  { return buffer_.empty(); }
    void                  clear()         { buffer_.clear(); }

private:
    std::vector<T> buffer_;
};

// ---------------------------------------------------------------------------
// EventRegistry — flush coordinator (stored as a World resource)
//
// Call register_queue<T>(world) once per event type during startup.
// Call flush_all() as the first Pre-Update step each frame.
// ---------------------------------------------------------------------------

class EventRegistry {
public:
    template<typename T>
    void register_queue(ecs::World& world) {
        world.set_resource(Events<T>{});
        flush_fns_.push_back([&world]() {
            if (auto* q = world.try_resource<Events<T>>()) q->clear();
        });
    }

    void flush_all() {
        for (auto& fn : flush_fns_) fn();
    }

private:
    std::vector<std::function<void()>> flush_fns_;
};

// ---------------------------------------------------------------------------
// Concrete event types
// ---------------------------------------------------------------------------

// Emitted by CharacterStateSystem when a jump fires.
// jump_number: 1 = first jump, 2 = double jump.
// impulse: vertical velocity applied (m/s).
struct JumpEvent {
    ecs::Entity entity;
    int         jump_number;
    float       impulse;
};

// Emitted by CharacterStateSystem on Airborne → Grounded transition.
struct LandEvent {
    ecs::Entity entity;
};
