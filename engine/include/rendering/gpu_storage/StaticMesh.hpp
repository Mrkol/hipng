#pragma once

#include <vulkan/vulkan.hpp>

#include "rendering/primitives/UniqueVmaBuffer.hpp"
#include "rendering/primitives/UniqueVmaImage.hpp"
#include "rendering/gpu_storage/Material.hpp"


struct Meshlet
{
	uint32_t vertex_offset;
	uint32_t index_offset;
	uint32_t index_count;
	glm::mat4x4 local_transform;
	Material* material;
};

struct StaticMesh
{
	UniqueVmaBuffer vertex_buffer;
	UniqueVmaBuffer index_buffer;
	std::vector<Meshlet> meshlets;
	std::vector<Material> materials;
};
