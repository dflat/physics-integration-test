# Game Engine — Claude Code Guidelines

## Project

3D physics-driven game engine prototype built on a custom ECS, Jolt Physics,
and Raylib. See `ARCH_STATE.md` for the current architectural design.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/demo
```

Debug build:
```bash
cmake -B build && cmake --build build
```

Run tests:
```bash
cmake --build build --target unit_tests && cd build && ctest
```

After every code change, **build and verify before considering the task done.**

## Dependencies

| Dependency | Source | Notes |
|------------|--------|-------|
| ECS | Git submodule (`extern/ecs`) | Header-only, actively co-developed |
| Jolt Physics | FetchContent (v5.2.0) | SSE4.2 enabled |
| Raylib | FetchContent (master) | Rendering + input |
| GLM | FetchContent | Math via ECS integration bridge |
| Catch2 | FetchContent (v3.4.0) | Unit tests only |

### ECS Submodule

The ECS library is a submodule at `extern/ecs`. When making changes:
- Changes to the ECS itself go in `extern/ecs/` and are committed/pushed
  to the ECS repo independently.
- After pushing ECS changes, update the submodule pointer in this repo:
  `git add extern/ecs && git commit -m "build: update ECS submodule"`

## Development Workflow

All feature work and architectural changes are planned via RFCs in
`docs/rfcs/`. See `docs/rfcs/README.md` for the pipeline and index.

1. **Plan:** Draft an RFC from `docs/rfcs/_TEMPLATE.md` into `00-proposals/`.
2. **Activate:** Move the RFC to `01-active/` when implementation begins.
3. **Implement:** Work incrementally. Each commit must build and run cleanly.
4. **Test:** New logic should have corresponding tests in `tests/`.
5. **Document:** Keep `ARCH_STATE.md` in sync with architectural changes.
6. **Complete:** Move the RFC to `02-implemented/` and update the index.

## Architecture

### Execution Pipeline

Four phases run each frame in strict order:

1. **Pre-Update** (variable dt): Input aggregation (keyboard, gamepad)
2. **Logic** (variable dt): Game logic systems (builder, camera, character)
3. **Physics** (fixed 60Hz): Jolt world step + `propagate_transforms()`
4. **Render** (variable dt): Raylib drawing (pure consumer, reads only)

Deferred commands are flushed between Logic and Physics phases.

### System Pattern

Systems are stateless classes with static methods:
- `Register(World&)` — one-time setup (lifecycle hooks via `on_add`/`on_remove`)
- `Update(World&, float dt)` — per-frame logic

Systems access shared state via ECS resources (`world.try_resource<T>()`).

### Component Design

Components are POD structs in `src/components.hpp`. Keep them data-only —
no methods, no logic, no inheritance.

## Code Style

- **C++20** (required by Jolt Physics).
- Systems go in `src/systems/` as header + source pairs.
- Components are centralized in `src/components.hpp`.
- Use `MathBridge` helpers for conversions between ECS/Jolt/Raylib math types.

## What Not To Do

- Don't add features without an RFC in `docs/rfcs/`.
- Don't put logic in components — components are data only.
- Don't perform structural ECS changes during iteration — use `world.deferred()`.
- Don't commit broken builds.
- Don't modify the ECS submodule without also building/testing the ECS repo.
