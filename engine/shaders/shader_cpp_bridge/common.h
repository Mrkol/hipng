#ifndef NG_COMMON_H
#define NG_COMMON_H

#ifdef __cplusplus

#   include <glm/mat4x4.hpp>
#   include <glm/vec4.hpp>
#   include <glm/vec3.hpp>
#   include <glm/vec2.hpp>

#   define mat4 glm::mat4x4
#   define vec4 glm::vec4
#   define vec3 glm::vec3
#   define vec2 glm::vec2

#   define DEFAULT(x) = x

#else

#   define DEFAULT(x)

#endif

struct GlobalUBO
{
    mat4 view;
    mat4 proj;
    vec4 ambientLight;
};

struct PointLight
{
	vec4 posAndOuterRadius;
    vec4 colorAndInnerRadius;
};

#endif // NG_COMMON_H
