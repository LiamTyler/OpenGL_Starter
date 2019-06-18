#version 430 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec3 gDiffuse;
layout (location = 3) out vec4 gSpecularExp;
layout (location = 4) out vec3 gEmissive;

in vec3 fragPosInWorldSpace;
in vec3 normalInWorldSpace;
in vec2 texCoord;

// uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform vec3 ke;
uniform float specular;

uniform bool textured;
uniform sampler2D diffuseTex;

void main() {
    gPosition = fragPosInWorldSpace;
    gNormal = normalize(normalInWorldSpace);
    gDiffuse = kd;
    gSpecularExp = vec4(ks, specular);
    gEmissive = ke;
    
    if (textured) {
        gDiffuse *= texture(diffuseTex, texCoord).xyz;
    }
}