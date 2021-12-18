#pragma once

#include "shader_cpp_bridge/static_mesh.h"


struct Material
{
	MaterialUBO ubo;
	UniqueVmaImage base_color;
	UniqueVmaImage occlusion_metalic_roughness;
};
