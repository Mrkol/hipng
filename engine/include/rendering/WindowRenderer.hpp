#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <unifex/task.hpp>
#include <flecs.h>
#include <function2/function2.hpp>

#include "rendering/primitives/UniqueVmaImage.hpp"


constexpr std::size_t MAX_FRAMES_IN_FLIGHT = 2;

using ResolutionProvider = fu2::unique_function<vk::Extent2D()>;

class WindowRenderer
{
public:
    struct RequiredQueues
    {
        uint32_t present;
        uint32_t graphics;
    };

    WindowRenderer(vk::PhysicalDevice physical_device, vk::Device device,
        VmaAllocator allocator, vk::UniqueSurfaceKHR surface, RequiredQueues queues,
        ResolutionProvider resolution_provider);

    void recreateSwapchain();

    unifex::task<void> renderFrame(float delta_seconds);

private:
    vk::PhysicalDevice physical_device_;
    vk::Device device_;
    VmaAllocator allocator_;
    vk::UniqueSurfaceKHR surface_;

    ResolutionProvider resolution_provider_;

    struct Queue {
        uint32_t index;
        vk::Queue queue;
    } present_queue_, graphics_queue_;


    struct SwapchainElement
    {
        vk::Image image;
        vk::UniqueImageView image_view;
        vk::Fence image_fence;
        vk::UniqueFramebuffer main_framebuffer;
        vk::UniqueFramebuffer gui_framebuffer;
        vk::UniqueCommandPool command_pool;
        vk::UniqueCommandBuffer command_buffer;
    };

    struct Swapchain
    {
        vk::UniqueSwapchainKHR swapchain;
        vk::Format format;
        vk::Extent2D extent;

        UniqueVmaImage depthbuffer;
        vk::UniqueImageView depthbuffer_view;

        vk::UniqueRenderPass render_pass;

        std::vector<SwapchainElement> elements;
    } swapchain_;
};
