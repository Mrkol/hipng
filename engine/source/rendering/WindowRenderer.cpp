#include "rendering/WindowRenderer.hpp"

#include <unifex/just.hpp>
#include "util/Assert.hpp"

#include <spdlog/spdlog.h>


constexpr auto DEPTHBUFFER_FORMAT = vk::Format::eD32Sfloat;

vk::SurfaceFormatKHR chose_surface_format(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface)
{
    auto formats = device.getSurfaceFormatsKHR(surface);

    if (formats.empty())
    {
        throw std::runtime_error("Device does not support any surface formats!");
    }

    auto found = std::find_if(formats.begin(), formats.end(),
                              [](const vk::SurfaceFormatKHR& format)
                              {
                                  return format.format == vk::Format::eB8G8R8A8Srgb
                                         && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
                              });

    return found != formats.end() ? *found : formats[0];
}

vk::PresentModeKHR chose_present_mode(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface)
{
    auto modes = device.getSurfacePresentModesKHR(surface);

    if (modes.empty())
    {
        throw std::runtime_error("Device doesn't support any present modes!");
    }

    return std::find(modes.begin(), modes.end(), vk::PresentModeKHR::eMailbox) != modes.end() ?
           vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}

vk::Extent2D chose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities, vk::Extent2D actualExtent)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

    return actualExtent;
}



WindowRenderer::WindowRenderer(vk::PhysicalDevice physical_device, vk::Device device,
        VmaAllocator allocator, vk::UniqueSurfaceKHR surface, RequiredQueues queues,
        ResolutionProvider resolution_provider)
    : physical_device_{physical_device}
    , device_{device}
    , allocator_{allocator}
    , surface_(std::move(surface))
    , resolution_provider_{std::move(resolution_provider)}
    , present_queue_{queues.present, device.getQueue(queues.present, 0)}
    , graphics_queue_{queues.graphics, device.getQueue(queues.graphics, 0)}
{
}

vk::ImageView WindowRenderer::acquire_next()
{
    auto res = device_.acquireNextImage2KHR(vk::AcquireNextImageInfoKHR{
        .swapchain = current_swapchain_.swapchain,
        .timeout = 1000000,
        .semaphore = {},
        .fence = {},
        .deviceMask = 0
    });

    if (res.result == vk::Result::eErrorOutOfDateKHR)
    {
	    
    }
    else if (res.result != vk::Result::eSuccess && res.result != vk::Result::eSuboptimalKHR)
    {
	    throw std::runtime_error("Swapchain lost!");
    }
}

WindowRenderer::SwapchainData WindowRenderer::create_swapchain() const
{
    auto surface_caps = physical_device_.getSurfaceCapabilitiesKHR(surface_.get());
    auto format = chose_surface_format(physical_device_, surface_.get());
    auto present_mode = chose_present_mode(physical_device_, surface_.get());
    auto extent = chose_swap_extent(surface_caps, resolution_provider_());

    uint32_t image_count = surface_caps.minImageCount + 1;

    if (surface_caps.maxImageCount > 0)
    {
        image_count = std::min(image_count, surface_caps.maxImageCount);
    }

    bool queues_differ = graphics_queue_.index != present_queue_.index;

    std::vector<uint32_t> queue_families;
    if (queues_differ)
    {
        queue_families = {graphics_queue_.index, present_queue_.index};
    }

    SwapchainData new_swapchain;

    new_swapchain.swapchain = device_.createSwapchainKHRUnique(vk::SwapchainCreateInfoKHR{
            {}, surface_.get(),
            image_count, format.format, format.colorSpace,
            extent, 1, vk::ImageUsageFlagBits::eColorAttachment,
            queues_differ ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
            static_cast<uint32_t>(queue_families.size()), queue_families.data(),
            surface_caps.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque,
            present_mode,
            /* clipped */ true,
            /* old swapchain */ current_swapchain_.swapchain.get()
        });

    new_swapchain.format = format.format;
    new_swapchain.extent = extent;

    auto imgs = device_.getSwapchainImagesKHR(new_swapchain.swapchain.get());
    
    new_swapchain.elements.resize(imgs.size());
    for (std::size_t i = 0; i < imgs.size(); ++i)
    {
        new_swapchain.elements[i].image = imgs[i];
        new_swapchain.elements[i].image_view = device_.createImageViewUnique(vk::ImageViewCreateInfo{
                {}, imgs[i], vk::ImageViewType::e2D, format.format,
                vk::ComponentMapping{},
                vk::ImageSubresourceRange{
                	.aspectMask = vk::ImageAspectFlagBits::eColor,
                	.baseMipLevel = 0,
                	.levelCount = 1,
                	.baseArrayLayer = 0,
                	.layerCount = 1
                }
			});
    }

    return new_swapchain;
}

unifex::task<void> WindowRenderer::render_frame(float delta_seconds)
{
    co_return;
}
