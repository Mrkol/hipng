#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "shader_cpp_bridge/static_mesh.h"

// INPUTS

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;


// OUTPUTS

layout(location = 0) out vs_out
{
    vec3 out_cs_position;
    vec3 out_cs_normal;
    vec2 out_uv;
};


// UNIFORMS

layout(set = 0, binding = 0) uniform GUBO { GlobalUBO global_ubo; };
// set 1 is per material
layout(set = 2, binding = 0) uniform OUBO { ObjectUBO object_ubo; };


void main() {
    vec4 screenspace_position = global_ubo.view * object_ubo.model * vec4(position.xyz, 1.0);

    out_cs_position = screenspace_position.xyz / screenspace_position.w;
    out_cs_normal = mat3(global_ubo.view) * mat3(object_ubo.normal) * normal.xyz;
    out_uv = vec2(position.w, normal.w);

    gl_Position = global_ubo.proj * screenspace_position;
}
