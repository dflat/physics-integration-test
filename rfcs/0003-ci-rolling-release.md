# RFC 0003: CI/CD Pipeline and Rolling Releases

*   **Status:** Implemented
*   **Date:** February 2026
*   **Author:** Gemini CLI Agent

## Summary
This RFC documents the design of the project's Continuous Integration (CI) and Continuous Deployment (CD) pipeline. It covers the automated Windows build verification, dependency caching for performance, and the "Latest Preview" rolling release system.

## Motivation
To ensure the project remains high-quality and accessible, we required:
1.  **Automated Cross-Platform Verification**: Ensuring that Linux-based development doesn't break Windows compatibility (MSVC).
2.  **Fast Turnaround**: Caching heavy dependencies (Jolt, Raylib, Catch2) to keep CI runs under 2 minutes.
3.  **Public Accessibility**: Providing a permanent, fixed link for anyone to download the latest playable binary without needing GitHub login or technical knowledge of Artifacts.

## Detailed Design

### 1. Automated Build & Test
The GitHub Action (`.github/workflows/windows-build.yml`) triggers on every push to the `master` branch.
- **Compiler**: Uses `windows-latest` (MSVC).
- **Quality Gate**: Runs `ctest` as a mandatory step. If unit tests fail, the build fails and no release is updated.

### 2. Dependency Caching
Uses `actions/cache@v4` to store `build/_deps`. 
- **Invalidation Logic**: The cache key is a hash of `CMakeLists.txt` and all source files in the `extern/` directory (`extern/**/*.hpp`, etc.). This ensures that updating the ECS submodule correctly triggers a clean build of dependencies.

### 3. Rolling Release System
Instead of manual versioning, the project uses a "Rolling Release" model:
- **Git Tag**: The `latest-preview` tag is force-moved to the current commit on every successful build.
- **Assets**: The `demo.exe` and `resources/` folder are automatically zipped into `physics-integration-test-windows.zip`.
- **Release**: Uses `softprops/action-gh-release` to overwrite the "Latest Preview" release with the new zip.

## Conclusion
The CI/CD pipeline ensures that the project is always in a "shippable" state. It provides both developers (via fast tests) and users (via rolling downloads) with a reliable source of truth for the project's progress.
