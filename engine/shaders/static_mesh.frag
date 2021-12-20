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


void main()
{
    vec4 color = texture(albedo, uv);
    vec4 omr = texture(occlusionMetalicRoughness, uv);

    vec3 normal = normalize(cs_normal);

    
    out_fragColor.rgb = omr.r * materialUbo.occlusionStrength * globalUbo.ambientLight.rgb;

    {
        PointLight light;
        light.posAndOuterRadius = vec4(-10, 10, 10, 0);
        light.colorAndInnerRadius = vec4(1, 1, 1, 0);

        vec3 light_dir =
            normalize(vec3(globalUbo.view * vec4(light.posAndOuterRadius.xyz, 1.0)) - cs_position);
        vec3 light_color = light.colorAndInnerRadius.rgb;

        out_fragColor.rgb +=
            BRDF(color.rgb,
                omr.g * materialUbo.roughnessFactor,
                omr.b * materialUbo.metallicFactor,
                light_dir,
                vec3(0, 0, -1),
                normal)
            * light_color
            * dot(light_dir, normal);
    }

    //out_fragColor.rgb = color.rgb * normal.y;
}
