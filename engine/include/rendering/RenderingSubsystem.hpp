#pragma once

#include <span>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <unifex/thread_unsafe_event_loop.hpp>
#include <unifex/task.hpp>
#include <unifex/async_manual_reset_event.hpp>

#include "concurrency/EventQueue.hpp"
#include "util/Assert.hpp"
#include "rendering/Window.hpp"
#include "rendering/FramePacket.hpp"
#include "rendering/TempForwardRenderer.hpp"
#include "rendering/primitives/InflightResource.hpp"
#include "rendering/gpu_storage/GpuStorageManager.hpp"


class RenderingSubsystem
{
public:
	struct CreateInfo
	{
	    vk::ApplicationInfo app_info;
	    std::span<const char* const> layers;
	    std::span<const char* const> extensions;
	};

    explicit RenderingSubsystem(CreateInfo info);

    /**
     * Warning: other public interface methods should NOT be called from this function.
     * That would lead to a asynchronous deadlock :)
     */
    [[nodiscard]] unifex::task<void> renderFrame(std::size_t frame_index, FramePacket packet);

    [[nodiscard]] vk::Instance getInstance() const { return instance_.get(); }

    /**
     * Initializes a window for vulkan rendering usage.
     */
    [[nodiscard]] unifex::task<void> makeVkWindow(vk::UniqueSurfaceKHR surface,
        ResolutionProvider resolution_provider, ImGuiContext* gui_context);

    [[nodiscard]] GpuStorageManager& getGpuStorageManager() { return *gpu_storage_manager_; }

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
    vk::UniqueInstance instance_;
    vk::UniqueDebugUtilsMessengerEXT debug_messenger_;
    vk::PhysicalDevice physical_device_;
    vk::UniqueDevice device_;
    std::unique_ptr<std::remove_pointer_t<VmaAllocator>, void(*)(VmaAllocator)> allocator_{nullptr, nullptr};

    std::vector<std::unique_ptr<Window>> windows_;
    std::vector<std::unique_ptr<IRenderer>> renderers_;

    std::unordered_map<Window*, IRenderer*> window_renderer_mapping_;

    // As per advice from the internet, we use the SAME queue for both graphics and present
    // no hardware exists where this would be a problem, apparently
    uint32_t graphics_queue_idx_;

    /**
     * This HAS To be locked by anyone who wants to mess with the rendering system.
     * Gets locked by each frame job. The implementation is guaranteed to be FIFO, so
     * frame reorderings don't happen. Plus we can use this as a RT command queue!!!!
     */
    unifex::async_mutex frame_mutex_;
    /**
     * Sometimes younger frames end faster than old frames. In such cases
     * incoming frames need to wait a bit so as not to create a data race on
     * inflight resources.
     */
    InflightResource<unifex::async_mutex> inflight_mutex_;

    struct Oneshot
    {
        explicit Oneshot(const auto& a, const auto& b) : pool{a}, fence{b} {}

	    // TODO: reconsider this
	    InflightResource<vk::UniqueCommandPool> pool;
	    InflightResource<vk::UniqueFence> fence;
    };

    std::optional<Oneshot> oneshot_;

    std::unique_ptr<GpuStorageManager> gpu_storage_manager_;
};
