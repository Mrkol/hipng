#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "metalic_roughness.h"


// INPUTS

layout(location = 0) in vs_out
{
    vec3 out_cs_position;
    vec3 out_cs_normal;
    vec2 out_uv;
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
    vec4 color = texture(albedo, out_uv);
    vec4 omr = texture(occlusionMetalicRoughness, out_uv);

    out_fragColor
        = omr.r * materialUbo.occlusionStrength * globalUbo.ambientLight
        + BRDF(color.rgb, omr.g * materialUbo.roughnessFactor, omr.b * materialUbo.metalnessFactor,
            vec3(0, 0, -1), out_cs_position, out_cs_normal);
}
