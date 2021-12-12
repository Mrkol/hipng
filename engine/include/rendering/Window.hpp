#pragma once

#include <optional>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <flecs.h>
#include <function2/function2.hpp>
#include <unifex/async_mutex.hpp>

#include "primitives/InflightResource.hpp"
#include "util/HeapArray.hpp"


using ResolutionProvider = fu2::unique_function<unifex::task<vk::Extent2D>() const>;


class Window
{
public:
	struct CreateInfo
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

    explicit Window(CreateInfo info);

    struct SwapchainImage
    {
        vk::ImageView view;
        vk::Semaphore available;
    };

    /**
     * In case this returns a nullopt, swapchain needs recreation at the nearest opportunity.
     * this_frame_done should be a task that completes whenever this frame's rendering is done
     * This is needed as a swapchain MIGHT return the same image twice in a row, therefore forcing us to limit
     * our concurrency
     */
    unifex::task<std::optional<SwapchainImage>> acquireNext(std::size_t frame_index);

    /**
     * Same as above, false means swapchain needs recreation.
     */
    bool present(vk::Semaphore wait, vk::ImageView which);

    std::vector<vk::ImageView> getAllImages();

    /**
     * Initiates the recreation process.
     * @return Resolution of the new window
     */
    unifex::task<std::optional<vk::Extent2D>> recreateSwapchain();
    /**
     * Finalizes the recreation process.
     * Should be called after everyone who wants to present to this image finished updating their data.
     * This is ugly, but other solutions are even worse.
     */
    void markSwapchainRecreated();

    void markImageFree(vk::ImageView which);


private:
    struct SwapchainElement
    {
        vk::Image image;
        vk::UniqueImageView image_view; // guarded by mtx
        unifex::async_mutex mtx;
    };
    struct SwapchainData
    {
        vk::UniqueSwapchainKHR swapchain;
        vk::Format format;
        vk::Extent2D extent;
        HeapArray<SwapchainElement> elements;
    };

    SwapchainData createSwapchain(vk::Extent2D resolution) const;

    uint32_t viewToIdx(vk::ImageView view);

private:
    vk::PhysicalDevice physical_device_;
    vk::Device device_;
    vk::UniqueSurfaceKHR surface_;

    ResolutionProvider resolution_provider_;

    uint32_t queue_family_;
    vk::Queue present_queue_;

    SwapchainData current_swapchain_{};
    InflightResource<vk::UniqueSemaphore> image_available_sem_;

    std::atomic<bool> swapchain_missing_{false};
};
