#pragma once

#include <vulkan/vulkan.hpp>

#include "rendering/primitives/InflightResource.hpp"


class StaticMeshRenderer
{
public:
	struct CreateInfo
	{
		vk::Device device;
		vk::DescriptorPool ds_pool;
		vk::RenderPass renderpass;
	};

	explicit StaticMeshRenderer(CreateInfo info);

	void render(std::size_t frame_index, vk::CommandBuffer cb);


private:
	vk::Device device_;
	vk::DescriptorPool ds_pool_;

	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniqueDescriptorSetLayout global_dsl_;
	vk::UniqueDescriptorSetLayout material_dsl_;
	vk::UniqueDescriptorSetLayout object_dsl_;
	vk::UniquePipeline pipeline_;

	struct PerFrameDses
	{
		vk::UniqueDescriptorSet global;
		vk::UniqueDescriptorSet material;
		vk::UniqueDescriptorSet object;
	};
	
	std::optional<InflightResource<vk::UniqueDescriptorSet>> per_frame_dses_;
};