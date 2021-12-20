#pragma once

#include <span>
#include <vulkan/vulkan.hpp>

#include "StaticMeshRenderer.hpp"
#include "rendering/gpu_storage/GpuStorageManager.hpp"
#include "rendering/IRenderer.hpp"
#include "rendering/primitives/InflightResource.hpp"


class TempForwardRenderer : public IRenderer
{
public:
	struct CreateInfo
	{
	    vk::Device device;
		VmaAllocator allocator;
	    vk::Queue graphics_queue;
	    uint32_t queue_family;
		GpuStorageManager* storage_manager;
	};

	explicit TempForwardRenderer(CreateInfo info);

	/**
	 *
	 * @param frame_index
	 * @param present_image -- Swapchain image that this operation should write to.
	 * Used to dispatch some resources (i.e. framebuffers)
	 * @param image_available -- Semaphore that gets signaled when we can start writing to the specified image view
	 * @return Synchronization primitives that will be signaled when the rendering finishes
	 */
	RenderingDone render(std::size_t frame_index, vk::ImageView present_image, vk::Semaphore image_available,
		FramePacket packet) override;

	void updatePresentationTarget(std::span<vk::ImageView> target, vk::Extent2D resolution) override;

private:
	vk::Device device_;
	VmaAllocator allocator_;
	vk::Queue graphics_queue_;
	InflightResource<vk::UniqueCommandPool> cb_pool_;
	GpuStorageManager* storage_manager_;

	vk::UniqueRenderPass renderpass_;
	InflightResource<vk::UniqueFence> rendering_done_fence_;
	InflightResource<vk::UniqueSemaphore> rendering_done_sem_;

	vk::Extent2D resolution_;
	std::unordered_map<VkImageView, vk::UniqueFramebuffer> framebuffer_;
	UniqueVmaImage depth_buffer_;
	vk::UniqueImageView depth_buffer_view_;

	InflightResource<vk::UniqueCommandBuffer> main_cb_;

	vk::UniqueDescriptorPool descriptor_pool_;

	std::unique_ptr<StaticMeshRenderer> static_mesh_renderer_;
};
