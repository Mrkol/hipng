#pragma once

#include <utility>
#include <optional>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <unifex/task.hpp>
#include <flecs.h>
#include <function2/function2.hpp>

#include "rendering/primitives/UniqueVmaImage.hpp"


using ResolutionProvider = fu2::unique_function<vk::Extent2D() const>;

struct WindowRendererCreateInfo
{
    vk::PhysicalDevice physical_device;
    vk::Device device;
    VmaAllocator allocator;
    vk::UniqueSurfaceKHR surface;
    ResolutionProvider resolution_provider;
    vk::Queue present_queue;
    // Reminder: this must be an all-in-one graphics + present queue
    uint32_t queue_family;
};

class WindowRenderer
{
public:
    explicit WindowRenderer(WindowRendererCreateInfo info);

    struct SwapchainImage
    {
        vk::ImageView view;
        vk::Semaphore available;
        uint32_t index;
    };

    std::optional<SwapchainImage> acquireNext();
    bool present(vk::Semaphore wait, uint32_t index);

    // Should only be called when the present queue is free of present requests
    void recreateSwapchain();


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

    SwapchainData createSwapchain() const;

private:
    vk::PhysicalDevice physical_device_;
    vk::Device device_;
    VmaAllocator allocator_;
    vk::UniqueSurfaceKHR surface_;

    ResolutionProvider resolution_provider_;

    uint32_t queue_family_;
    vk::Queue present_queue_;

    SwapchainData current_swapchain_;
};
