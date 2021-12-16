#ifndef NG_STATIC_MESH_H
#define NG_STATIC_MESH_H

#include "common.h"


struct ObjectUBO
{
    mat4 model;
    mat4 normal;
};

struct MaterialUBO
{
    float baseColorFactor DEFAULT(1.f);
    float metallicFactor DEFAULT(1.f);
    float roughnessFactor DEFAULT(1.f);
    float occlusionStrength DEFAULT(1.f);
};


#endif // NG_STATIC_MESH_H
