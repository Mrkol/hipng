#pragma once

#include <span>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <unifex/thread_unsafe_event_loop.hpp>
#include <unifex/task.hpp>
#include <unifex/async_manual_reset_event.hpp>

#include "concurrency/EventQueue.hpp"
#include "util/Assert.hpp"
#include "rendering/WindowRenderer.hpp"
#include "rendering/TempForwardRenderer.hpp"
#include "rendering/primitives/InflightResource.hpp"


struct GlobalRendererCreateInfo
{
    vk::ApplicationInfo app_info;
    std::span<const char* const> layers;
    std::span<const char* const> extensions;
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

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	    VkDebugUtilsMessageTypeFlagsEXT messageType,
	    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	    void* pUserData);

private:
    EventQueue command_queue_;

    vk::UniqueInstance instance_;
    vk::UniqueDebugUtilsMessengerEXT debug_messenger_;
    vk::PhysicalDevice physical_device_;
    vk::UniqueDevice device_;
    std::unique_ptr<std::remove_pointer_t<VmaAllocator>, void(*)(VmaAllocator)> allocator_{nullptr, nullptr};

    std::vector<std::unique_ptr<WindowRenderer>> window_renderers_;

    // As per advice from the internet, we use the SAME queue for both graphics and present
    // no hardware exists where this would be a problem, apparently
    uint32_t graphics_queue_idx_;
    
    InflightResource<unifex::async_manual_reset_event> frame_submitted_;

    std::unique_ptr<TempForwardRenderer> forward_renderer_;
};
