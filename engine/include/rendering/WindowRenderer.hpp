#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <unifex/task.hpp>
#include <flecs.h>
#include <function2/function2.hpp>

#include "rendering/primitives/UniqueVmaImage.hpp"


using ResolutionProvider = fu2::unique_function<vk::Extent2D() const>;

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

    vk::ImageView acquire_next();

    unifex::task<void> render_frame(float delta_seconds);


private:
    struct SwapchainElement
    {
        vk::Image image;
        vk::UniqueImageView image_view;
        vk::Fence image_fence;
    };
    struct SwapchainData
    {
        vk::UniqueSwapchainKHR swapchain;
        vk::Format format;
        vk::Extent2D extent;
        std::vector<SwapchainElement> elements;
    };

    SwapchainData create_swapchain() const;

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

    SwapchainData current_swapchain_;
};
