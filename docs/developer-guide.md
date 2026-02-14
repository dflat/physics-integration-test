# Developer Guide

This guide provides technical details on the build system and the workflow for managing the custom ECS dependency.

## Build System Architecture

The project uses **CMake** (3.18+) as the build orchestrator. It manages three types of dependencies:

1.  **Git Submodules (`extern/ecs`):** Used for the core ECS engine because it is under active development. This allows the project to point to a specific commit while making it easy to contribute changes back.
2.  **FetchContent (Jolt, Raylib, GLM):** Used for stable, external third-party libraries. These are downloaded and configured automatically during the CMake "Configure" step.
3.  **Local Resources:** The `resources/` folder (containing shaders) is essential for execution. A CMake `POST_BUILD` command ensures this folder is copied to the same directory as the `demo` executable on all platforms.

## ECS Submodule Workflow

Since the ECS is a submodule in `extern/ecs`, you must handle it differently than standard files.

### 1. Initial Setup
When cloning for the first time, use:
```bash
git clone --recursive <url>
```
If you already cloned without submodules:
```bash
git submodule update --init --recursive
```

### 2. Pulling Updates from ECS Upstream
If the independent ECS repository has new features you want to bring into this project:
```bash
# Update the submodule to the latest commit on its tracking branch
git submodule update --remote extern/ecs

# You must then commit the new pointer in the main repo
git add extern/ecs
git commit -m "build: update ECS submodule to latest upstream"
```

### 3. Pushing Changes to ECS Upstream
If you fix a bug or add a feature to the ECS while working inside this project:
```bash
cd extern/ecs

# 1. Make your changes
# 2. Commit them locally within the submodule
git add .
git commit -m "feat: add new feature to ECS"

# 3. Push to the ECS remote (assuming you have write access)
git push origin HEAD:master 

# 4. Return to the root project and update the pointer
cd ../..
git add extern/ecs
git commit -m "build: sync ECS submodule pointer with upstream changes"
```

## Troubleshooting Build Issues

- **Missing Shaders:** If the app crashes on startup, ensure the `resources/` folder exists next to the `demo` binary. Re-run the build to trigger the copy command.
- **Header Conflicts:** The project uses C++20. Ensure your compiler is up to date. Jolt Physics is particularly sensitive to C++ standard mismatches.
- **Submodule out of sync:** If `git status` shows `extern/ecs (modified content, untracked content)`, run `git submodule update --init` to reset the pointer to the version expected by the main project.
