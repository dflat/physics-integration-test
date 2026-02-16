# RFC 0002: Quality Assurance and Unit Testing Suite

*   **Status:** Implemented
*   **Date:** February 2026
*   **Author:** Gemini CLI Agent

## Summary
This RFC defines the implementation of a modern unit testing infrastructure for the project. It introduces the **Catch2** framework and establishes a pattern for decoupling mathematical and logical "kernels" from engine side-effects (like physics simulation or rendering) to enable automated verification.

## Motivation
As the complexity of the character controller and camera logic grew, manual verification became insufficient. We needed a way to:
1.  Verify core mathematical building blocks (angle normalization, follow weighting).
2.  Prevent regressions in character movement (double-jump windows, coyote time).
3.  Ensure cross-platform consistency of logic across different compilers.

## Detailed Design

### 1. Framework Choice: Catch2 v3
Catch2 was selected for its modern C++, "batteries-included" approach, and excellent CMake integration. It is fetched automatically via `FetchContent`.

### 2. Logic Extraction Strategy
To make the engine testable, we introduced `src/math_util.hpp`. This header contains "pure" functions that take primitive inputs and return values without relying on ECS state or external libraries. 
- Example: `engine::math::normalize_angle` and `engine::math::calculate_follow_angle`.

### 3. Test Organization
- **`tests/main.cpp`**: Minimal entry point using `Catch2::Catch2WithMain`.
- **`tests/logic_tests.cpp`**: Contains specific test cases for mathematical utilities.
- **`unit_tests` Target**: A dedicated executable defined in CMake that runs the entire suite via `ctest`.

## Implementation Notes
During implementation, we identified and fixed a discrepancy where `atan2` results could oscillate between `PI` and `-PI` on different platforms. The tests now use `normalize_angle` to ensure consistent verification across environments.

## Conclusion
The introduction of a testing suite marks a transition from a "prototype" to a "reliable engine." All new mathematical and logical features should now be accompanied by corresponding unit tests in the `tests/` directory.
