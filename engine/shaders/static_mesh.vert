#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "shader_cpp_bridge/static_mesh.h"

// INPUTS

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 normal;


// OUTPUTS

layout(location = 0) out vec3 out_ss_position;
layout(location = 1) out vec3 out_ss_normal;
layout(location = 2) out vec2 out_uv;


// UNIFORMS

layout(set = 0, binding = 0) uniform GUBO { GlobalUBO global_ubo; };
layout(set = 1, binding = 0) uniform OUBO { ObjectUBO object_ubo; };


void main() {
    vec4 screenspace_position = global_ubo.view * object_ubo.model * vec4(position.xyz, 1.0);

    out_ss_position = screenspace_position.xyz / screenspace_position.w;
    out_ss_normal = mat3(object_ubo.normal) * normal.xyz;
    out_uv = vec2(position.w, normal.w);

    gl_Position = global_ubo.proj * screenspace_position;
}
