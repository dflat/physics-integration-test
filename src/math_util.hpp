#pragma once
#include <cmath>
#include <algorithm>

namespace engine::math {

/**
 * @brief Normalizes an angle into the range [-PI, PI].
 */
inline float normalize_angle(float angle) {
    const float pi = 3.1415926535f;
    while (angle < -pi) angle += 2.0f * pi;
    while (angle >  pi) angle -= 2.0f * pi;
    return angle;
}

/**
 * @brief Calculates the target orbit angle (phi) to be BEHIND a 2D direction.
 */
inline float calculate_follow_angle(float dx, float dz) {
    // In our Sin/Cos orbit system, we want to be opposite the movement
    return std::atan2(-dx, -dz);
}

/**
 * @brief Calculates the alignment between two 2D directions.
 * @return 1.0 if perfectly aligned, -1.0 if perfectly opposite.
 */
inline float calculate_alignment(float dx1, float dz1, float dx2, float dz2) {
    float mag1 = std::sqrt(dx1*dx1 + dz1*dz1);
    float mag2 = std::sqrt(dx2*dx2 + dz2*dz2);
    if (mag1 < 0.001f || mag2 < 0.001f) return 0.0f;
    return (dx1 * dx2 + dz1 * dz2) / (mag1 * mag2);
}

} // namespace engine::math
