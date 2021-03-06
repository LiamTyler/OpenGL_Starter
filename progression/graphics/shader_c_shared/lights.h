#pragma once

#include "graphics/shader_c_shared/defines.h"

PG_NAMESPACE_BEGIN
PG_GPU_NAMESPACE_BEGIN

struct DirectionalLight
{
    VEC4 colorAndIntensity; // x,y,z = color, w = intensity
    VEC4 direction;         // x,y,z = direction, w = padding
    UVEC4 shadowMapIndex;   // x = shadowMapIndex, y,z,w = padding
};
    
struct PointLight
{
    VEC4 colorAndIntensity; // x,y,z = color, w = intensity
    VEC4 positionAndRadius; // x,y,z = position, w = radius
};  

struct SpotLight
{
    VEC4 colorAndIntensity;  // x,y,z = color, w = intensity
    VEC4 positionAndRadius;  // x,y,z = position, w = radius
    VEC4 directionAndCutoff; // x,y,z = direction, w = cutoff
};

PG_GPU_NAMESPACE_END
PG_NAMESPACE_END