#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "metalic_roughness.h"
#include "shader_cpp_bridge/static_mesh.h"


// INPUTS

layout(location = 0) in vs_out
{
    vec3 cs_position;
    vec3 cs_normal;
    vec2 uv;
};


// OUTPUTS

layout(location = 0) out vec4 out_fragColor;


// UNIFORMS

layout(set = 0, binding = 0) uniform GUBO { GlobalUBO globalUbo; };

layout(set = 1, binding = 0) uniform MUBO { MaterialUBO materialUbo; };
layout(set = 1, binding = 1) uniform sampler2D albedo;
layout(set = 1, binding = 2) uniform sampler2D occlusionMetalicRoughness;
layout(set = 1, binding = 3) uniform sampler2D normal;


void main()
{
    vec4 color = texture(albedo, uv);
    vec4 omr = texture(occlusionMetalicRoughness, uv);

    out_fragColor.rgb
        = omr.r * materialUbo.occlusionStrength * globalUbo.ambientLight.rgb
        + BRDF(color.rgb,
            omr.g * materialUbo.roughnessFactor,
            omr.b * materialUbo.metallicFactor,
            vec3(0, 0, -1), cs_position,
            cs_normal);
}
