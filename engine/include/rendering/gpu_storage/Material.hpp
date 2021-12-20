#pragma once

#include "shader_cpp_bridge/static_mesh.h"


struct Material
{
	MaterialUBO ubo;
	UniqueVmaImage base_color;
	vk::UniqueImageView base_color_view;
	UniqueVmaImage occlusion_metalic_roughness;
	vk::UniqueImageView occlusion_metalic_roughness_view;
};
