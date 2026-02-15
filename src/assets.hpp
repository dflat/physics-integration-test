#pragma once
#include <raylib.h>
#include <raymath.h>

struct AssetResource {
    Shader lighting_shader;
    
    // Uniform Locations
    int lightDirLoc;
    int lightColorLoc;
    int ambientLoc;
    int playerPosLoc;
    int shadowRadiusLoc;
    int shadowIntensityLoc;

    void load() {
        lighting_shader = LoadShader("resources/shaders/lighting.vs", "resources/shaders/lighting.fs");
        lightDirLoc = GetShaderLocation(lighting_shader, "lightDir");
        lightColorLoc = GetShaderLocation(lighting_shader, "lightColor");
        ambientLoc = GetShaderLocation(lighting_shader, "ambient");
        playerPosLoc = GetShaderLocation(lighting_shader, "playerPos");
        shadowRadiusLoc = GetShaderLocation(lighting_shader, "shadowRadius");
        shadowIntensityLoc = GetShaderLocation(lighting_shader, "shadowIntensity");

        // Set Static Defaults
        Vector3 dir = Vector3Normalize({-0.5f, -1.0f, -0.3f});
        SetShaderValue(lighting_shader, lightDirLoc, &dir, SHADER_UNIFORM_VEC3);
        Vector4 color = {1.0f, 1.0f, 0.9f, 1.0f};
        SetShaderValue(lighting_shader, lightColorLoc, &color, SHADER_UNIFORM_VEC4);
        Vector4 ambient = {0.3f, 0.3f, 0.35f, 1.0f};
        SetShaderValue(lighting_shader, ambientLoc, &ambient, SHADER_UNIFORM_VEC4);
    }

    void unload() {
        UnloadShader(lighting_shader);
    }
};
