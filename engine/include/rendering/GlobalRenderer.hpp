#pragma once

#include <span>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <unifex/thread_unsafe_event_loop.hpp>
#include <unifex/task.hpp>
#include <unifex/async_manual_reset_event.hpp>

#include "util/Assert.hpp"
#include "util/HeapArray.hpp"
#include "rendering/WindowRenderer.hpp"


struct GlobalRendererCreateInfo
{
    vk::ApplicationInfo app_info;
    std::span<const char* const> layers;
    std::span<const char* const> extensions;
    uint32_t max_frames_in_flight;
};

class GlobalRenderer
{
public:
    explicit GlobalRenderer(GlobalRendererCreateInfo info);

    [[nodiscard]] unifex::task<void> renderFrame(std::size_t frame_index);

    [[nodiscard]] vk::Instance getInstance() const { return instance_.get(); }

    [[nodiscard]] WindowRenderer* makeWindowRenderer(vk::UniqueSurfaceKHR surface, ResolutionProvider resolution_provider);

private:
    template<std::invocable<const vk::QueueFamilyProperties&> F>
    uint32_t findQueue(F&& f) const
    {
        static auto queues = physical_device_.getQueueFamilyProperties();

        auto it = std::find_if(queues.begin(), queues.end(), (F&&) f);

        NG_VERIFYF(it != queues.end(), "GPU does not support graphics!");

        return static_cast<uint32_t>(it - queues.begin());
    }

private:
    uint32_t max_frames_in_flight_;

    vk::UniqueInstance instance_;
    vk::PhysicalDevice physical_device_;
    vk::UniqueDevice device_;
    std::unique_ptr<std::remove_pointer_t<VmaAllocator>, void(*)(VmaAllocator)> allocator_{nullptr, nullptr};

    std::vector<std::unique_ptr<WindowRenderer>> window_renderers_;

    // As per advice from the internet, we use the SAME queue for both graphics and present
    // no hardware exists where this would be a problem, apparently
    uint32_t graphics_queue_idx_;

    std::vector<vk::UniqueFence> inflight_fences_;
    HeapArray<unifex::async_manual_reset_event> frame_submitted_events_;
};
