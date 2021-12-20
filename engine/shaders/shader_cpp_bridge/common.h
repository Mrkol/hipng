#ifndef NG_COMMON_H
#define NG_COMMON_H

#ifdef __cplusplus

#   include <glm/mat4x4.hpp>
#   include <glm/vec4.hpp>
#   include <glm/vec3.hpp>
#   include <glm/vec2.hpp>

#   define ngmat4 glm::mat4x4
#   define ngvec4 glm::vec4
#   define ngvec3 glm::vec3
#   define ngvec2 glm::vec2

#   define DEFAULT(x) = x

#else

#   define ngmat4 mat4
#   define ngvec4 vec4
#   define ngvec3 vec3
#   define ngvec2 vec2

#   define DEFAULT(x)

#endif

struct GlobalUBO
{
    ngmat4 view;
    ngmat4 proj;
    ngvec4 ambientLight;
};

struct PointLight
{
	ngvec4 posAndOuterRadius;
    ngvec4 colorAndInnerRadius;
};

#endif // NG_COMMON_H
