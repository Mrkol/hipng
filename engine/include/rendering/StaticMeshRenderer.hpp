#pragma once

#include <vulkan/vulkan.hpp>

#include "primitives/UniqueVmaBuffer.hpp"
#include "primitives/UniqueVmaImage.hpp"
#include "rendering/primitives/InflightResource.hpp"
#include "rendering/FramePacket.hpp"


class GpuStorageManager;

class StaticMeshRenderer
{
public:
	struct CreateInfo
	{
		vk::Device device;
		VmaAllocator allocator;
		GpuStorageManager* storage_manager;
		vk::DescriptorPool ds_pool;
		vk::RenderPass renderpass;
	};

	explicit StaticMeshRenderer(CreateInfo info);

	void render(std::size_t frame_index, vk::CommandBuffer cb, const FramePacket& packet);


private:
	vk::Device device_;
	VmaAllocator allocator_;
	vk::DescriptorPool ds_pool_;
	GpuStorageManager* storage_manager_;

	vk::UniqueSampler sampler_;

	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniqueDescriptorSetLayout global_dsl_;
	vk::UniqueDescriptorSetLayout material_dsl_;
	vk::UniqueDescriptorSetLayout object_dsl_;
	vk::UniquePipeline pipeline_;

	struct PerFrameDses
	{
		vk::UniqueDescriptorSet global;
		UniqueVmaBuffer global_ubo;
		std::vector<vk::UniqueDescriptorSet> materials;
		UniqueVmaBuffer material_ubos;
		// one due to dynamic offsets
		vk::UniqueDescriptorSet object;
		UniqueVmaBuffer object_ubos;
	};
	
	InflightResource<std::optional<PerFrameDses>> per_frame_dses_;
};