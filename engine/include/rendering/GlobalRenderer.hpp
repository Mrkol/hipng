#pragma once

#include <span>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <unifex/thread_unsafe_event_loop.hpp>
#include <unifex/task.hpp>

#include "rendering/WindowRenderer.hpp"

struct QueueFamilyIndices
{
    uint32_t graphics_queue_idx{};
    uint32_t presentation_queue_idx{};
};

class GlobalRenderer
{
public:
    GlobalRenderer(vk::ApplicationInfo info, std::span<const char* const> layers, std::span<const char* const> extensions);

    [[nodiscard]] unifex::task<void> renderFrame(float delta_seconds);

    [[nodiscard]] vk::Instance getInstance() const { return instance_.get(); }

    [[nodiscard]] WindowRenderer* makeWindowRenderer(vk::UniqueSurfaceKHR surface, ResolutionProvider resolution_provider);

private:
    void initializeDevice(vk::SurfaceKHR surface);

private:
    unifex::thread_unsafe_event_loop command_queue_;

    QueueFamilyIndices queueFamilyIndices_;

    vk::UniqueInstance instance_;
    vk::PhysicalDevice physical_device_;
    vk::UniqueDevice device_;
    std::unique_ptr<std::remove_pointer_t<VmaAllocator>, void(*)(VmaAllocator)> allocator_{nullptr, nullptr};

    std::vector<std::unique_ptr<WindowRenderer>> window_renderers_;
};
