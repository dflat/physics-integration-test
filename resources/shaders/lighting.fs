#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragPosition;

out vec4 finalColor;

uniform vec3 lightDir;
uniform vec4 lightColor;
uniform vec4 ambient;

// Player Shadow Uniforms
uniform vec3 playerPos;
uniform float shadowRadius;
uniform float shadowIntensity;

void main() {
    // Basic Lighting
    float diff = max(dot(fragNormal, -lightDir), 0.0);
    vec4 diffuse = diff * lightColor;
    
    vec4 result = (ambient + diffuse) * fragColor;

    // --- Dynamic Player Shadow (Projected) ---
    vec2 fragXZ = fragPosition.xz;
    vec2 playerXZ = playerPos.xz;
    float dist = distance(fragXZ, playerXZ);

    // Only cast shadow if the fragment is below the player
    float verticalDist = playerPos.y - fragPosition.y;
    
    if (verticalDist > -0.1 && verticalDist < 15.0 && dist < shadowRadius) {
        float edgeSmooth = 0.1;
        float shadowFactor = smoothstep(shadowRadius, shadowRadius - edgeSmooth, dist);
        
        // Vertical falloff (shadow fades as player gets higher)
        float heightFactor = 1.0 - clamp(verticalDist / 15.0, 0.0, 1.0);
        
        float finalShadow = shadowFactor * heightFactor * shadowIntensity;
        result.rgb *= (1.0 - finalShadow);
    }

    finalColor = result;
}
