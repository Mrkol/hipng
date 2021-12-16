#ifndef NG_COMMON_H
#define NG_COMMON_H

#ifdef __cplusplus

#include <klein/mat4x4.hpp>

#define mat4 kln::mat4x4


#endif

struct GlobalUBO
{
    mat4 view;
    mat4 proj;
};


#endif // NG_COMMON_H
