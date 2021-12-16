#pragma once

#include <vulkan/vulkan.hpp>

#include "rendering/primitives/UniqueVmaBuffer.hpp"
#include "rendering/primitives/UniqueVmaImage.hpp"


class StaticMesh
{
	UniqueVmaBuffer vertex_buffer;
	UniqueVmaBuffer index_buffer;
	UniqueVmaImage base_color;
	UniqueVmaImage occlusion_metalic_roughness;
};
