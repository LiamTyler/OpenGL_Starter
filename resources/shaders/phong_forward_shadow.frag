#version 430 core

#define EPSILON 0.000001

struct Light {
    vec3 pos;
    float rSquared;
    vec3 color;
    float innerCutoff;
    vec3 dir;
    float outerCutoff;
};

in vec3 fragPosInWorldSpace;
in vec3 normalInWorldSpace;
in vec2 texCoord;

uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform vec3 ke;
uniform float specular;

uniform vec3 cameraPos;
uniform bool textured;
uniform sampler2D diffuseTex;

// shadow data
uniform Light shadowLight;
uniform samplerCube depthCube;

layout (location = 0) out vec4 finalColor;

float attenuate(in const float distSquared, in const float radiusSquared) {
    float frac = distSquared / radiusSquared;
    float atten = max(0, 1 - frac * frac);
    return (atten * atten) / (1.0 + distSquared);
}

float ShadowAmount(in const vec3 n, in const vec3 dirToLight) {

    vec3 fragToLight = fragPosInWorldSpace - shadowLight.pos;
    // use the light to fragment vector to sample from the depth map    
    float closestDepth = texture(depthCube, fragToLight).r;
    // it is currently in linear range between [0,1]. Re-transform back to original value
    closestDepth *= sqrt(shadowLight.rSquared);
    // now get current linear depth as the length between the fragment and light position
    float currentDepth = length(fragToLight);
    // now test for shadows
    float bias = 0.05; 
    float shadow = currentDepth -  bias > closestDepth ? 1.0 : 0.0;
    return shadow;
}

vec3 ShadowLighting(in const vec3 n, in const vec3 e, in const vec3 diffuseColor) {
    vec3 color = vec3(0, 0, 0);
    return vec3(1, 0, 0);
    // return shadowLight.color;
    
    vec3 vertToLight = shadowLight.pos - fragPosInWorldSpace;
    vec3 l = normalize(vertToLight);
    vec3 h = normalize(l + e);
    float d2 = dot(vertToLight, vertToLight);
    float atten = 1.0; //attenuate(d2, shadowLight.rSquared);
            
    color += shadowLight.color;//* diffuseColor * max(0.0, dot(l, n));
    // color += atten * shadowLight.color * diffuseColor * max(0.0, dot(l, n));
    // if (dot(l, n) > EPSILON)
    //     color += atten * shadowLight.color * ks * pow(max(dot(h, n), 0.0), 4*specular);
    
    // color *= ShadowAmount(n, l);
    
    return color;
}

void main() {
    finalColor = vec4(1, 0, 0, 1.0);
    return;
    // vec3 n = normalize(normalInWorldSpace);
    // vec3 e = normalize(cameraPos - fragPosInWorldSpace);
    
    // vec3 diffuseColor = kd;
    // if (textured) {
    //     diffuseColor *= texture(diffuseTex, texCoord).xyz;
    // }

    // vec3 outColor = ke + ka*vec3(0);
    // outColor += ShadowLighting(n, e, diffuseColor);
    
}
