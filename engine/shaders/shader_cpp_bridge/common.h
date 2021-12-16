#ifndef NG_COMMON_H
#define NG_COMMON_H

#ifdef __cplusplus

#   include <klein/mat4x4.hpp>
#   include <klein/direction.hpp>

#   define mat4 kln::mat4x4
// Vec4 generaly has no meaning in shaders, it can store basically anything
// therefore we use a general 4-float array in C++
#   define vec4 std::array<float, 4>

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
