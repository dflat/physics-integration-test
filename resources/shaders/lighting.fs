#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragPosition;

out vec4 finalColor;

uniform vec3 lightDir;
uniform vec4 lightColor;
uniform vec4 ambient;

void main() {
    float diff = max(dot(fragNormal, -lightDir), 0.0);
    vec4 diffuse = diff * lightColor;
    finalColor = (ambient + diffuse) * fragColor;
}
