#version 430 core

#define EPSILON 0.000001

in vec2 UV;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gDiffuse;
uniform sampler2D gSpecularExp;
uniform sampler2D gEmissive;

uniform sampler2D shadowMap;
uniform bool shadows;
uniform mat4 LSM;

uniform vec3 cameraPos;

uniform vec3 lightDir;
uniform vec3 lightColor;

out vec4 finalColor;

// return how much the fragment is 'in' the shadow.
// 0 == not at all
// 1 == fully shadowed
float shadowAmount(const in vec3 fragPos, const in vec3 n, const in vec3 l) {
    vec4 fragPosInLightSpace = LSM * vec4(fragPos, 1);
    vec3 ndc = fragPosInLightSpace.xyz / fragPosInLightSpace.w;
    vec3 projCoords = 0.5 * ndc + vec3(0.5);
    float currentDepth = projCoords.z;
    // To account for when the fragment is shadow's projection matrix doesn't reach far enough
    // so the depth would be > 1 and always be in shadow
    if (currentDepth > 1.0)
        return 0;

    float cosTheta = max(0.0, dot(n, l));
    float bias = max(0.008 * (1.0 - cosTheta), 0.0005);
    // float bias = 0.0008*tan(acos(cosTheta));
    bias = clamp(bias, 0.0, 0.01);

    return currentDepth - bias > texture(shadowMap, projCoords.xy).r ? 1.0 : 0.0;
}

void main() {    
    vec3 fragPos      = texture(gPosition, UV).rgb;
    vec3 n            = texture(gNormal, UV).rgb;
    vec3 diffuseColor = texture(gDiffuse, UV).rgb;
    vec4 specExp      = texture(gSpecularExp, UV);
    vec3 ke           = texture(gEmissive, UV).rgb;

    vec3 e = normalize(cameraPos - fragPos);

    vec3 outColor = ke;
    
    vec3 l = normalize(lightDir);
    vec3 h = normalize(l + e);
    outColor += lightColor * diffuseColor * max(0.0, dot(l, n));
    if (dot(l, n) > EPSILON)
        outColor += lightColor * specExp.rgb * pow(max(dot(h, n), 0.0), 4*specExp.a);        

    if (shadows) {
        outColor *= (1.0f - shadowAmount(fragPos, n, l));
    }
    // outColor += lightColor * ka;
    
    finalColor.rgb = outColor;
    finalColor.a   = 1.0;
}