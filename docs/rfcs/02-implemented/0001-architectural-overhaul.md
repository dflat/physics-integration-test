# RFC 0001: Architectural Modularization, Pipelines, and Resource Migration

*   **Status:** Implemented
*   **Date:** February 2026
*   **Author:** Gemini CLI Agent

## Summary
This RFC documents the comprehensive refactoring of the physics-integration-test project from a monolithic, header-heavy prototype into a modular, phase-based ECS engine. The overhaul was executed in three distinct phases: Modularization, Pipeline Orchestration, and Resource Migration.

## Motivation
The initial prototype suffered from several architectural deficiencies:
1.  **Logic-in-Headers:** System implementations were in `.hpp` files, leading to slow compile times and potential circular dependencies.
2.  **Brittle Orchestration:** Systems were manually called in `main.cpp`, making the execution order fragile and prone to bugs.
3.  **Variable Physics Step:** Physics simulation was tied to the frame rate, causing inconsistent behavior across different hardware.
4.  **Improper ECS Patterns:** Singleton entities and `static` local variables were used instead of global ECS Resources, complicating state management.

## Detailed Design

### Phase 1: Modularization
Every logic system was split into a header/source pair. 
- Headers now only contain the class interface.
- Source files (`.cpp`) contain the implementation logic.
- This decoupled the system dependencies and improved build modularity.

### Phase 2: Execution Pipelines & Fixed Timestep
Introduced an `ecs::Pipeline` utility to categorize work into distinct phases:
- **PreUpdate:** Input aggregation.
- **Logic:** Character movement, camera follow, building.
- **Physics (Fixed Step):** 60Hz fixed-timestep simulation loop using an accumulator.
- **Render:** Pure presentation logic.

The pipeline automatically handles `world.deferred().flush()` between phases, ensuring structural changes (like spawning a platform) are synchronized before simulation.

### Phase 3: Resource Migration
Migrated unique global state to the ECS Resource API:
- **`MainCamera`**: Moved from a singleton entity to a World Resource.
- **`AssetResource`**: Centralized shader management, replacing lazy-loading `static` variables in the renderer.
- **`WorldTag`**: Introduced to explicitly identify game entities for safe scene resets without affecting global resources.

## Deployment Strategy
The build system was updated to link against Windows system libraries and deploy resources automatically via `POST_BUILD` commands. A GitHub Action was established to provide continuous verification of the Windows build and produce rolling release artifacts.

## Conclusion
These changes provide a stable, standard-compliant foundation for future development. The engine is now robust against frame rate fluctuations, safe for structural modifications during iteration, and follows industry-standard C++ and ECS practices.
