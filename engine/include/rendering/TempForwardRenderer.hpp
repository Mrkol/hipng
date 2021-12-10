#pragma once

#include <span>
#include <vulkan/vulkan.hpp>

#include "rendering/primitives/InflightResource.hpp"


class TempForwardRenderer
{
public:
	struct CreateInfo
	{
	    vk::Device device;
	    vk::Queue graphics_queue;
	    uint32_t queue_family;
	};

	explicit TempForwardRenderer(CreateInfo info);

	struct RenderingDone
	{
		vk::Semaphore sem;
		vk::Fence fence;
	};

	/**
	 * Rationale for passing in the present image view is that when it changes, some framebuffers need
	 * recreation. Keeping track of this should be the renderer's responsibility, the view is the only
	 * thing bridging the windowing and rendering systems.
	 */
	RenderingDone render(std::size_t frame_index, vk::ImageView present_image, vk::Semaphore image_available);

	void updatePresentationTarget(std::span<vk::ImageView> target, vk::Extent2D resolution);

private:
	vk::Device device_;
	vk::Queue graphics_queue_;
	InflightResource<vk::UniqueCommandPool> cb_pool_;

	vk::UniqueRenderPass renderpass_;
	InflightResource<vk::UniqueFence> rendering_done_fence_;
	InflightResource<vk::UniqueSemaphore> rendering_done_sem_;

	vk::Extent2D resolution_;
	std::unordered_map<VkImageView, vk::UniqueFramebuffer> framebuffer_;

	InflightResource<vk::UniqueCommandBuffer> main_cb_;
};
