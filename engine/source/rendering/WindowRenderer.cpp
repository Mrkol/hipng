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



WindowRenderer::WindowRenderer(WindowRendererCreateInfo info)
    : physical_device_{info.physical_device}
    , device_{info.device}
    , allocator_{info.allocator}
    , surface_(std::move(info.surface))
    , resolution_provider_{std::move(info.resolution_provider)}
    , present_queue_{info.present_queue}
    , queue_family_{info.queue_family}
{
}

auto WindowRenderer::acquireNext() -> std::optional<SwapchainImage>
{
    auto res = device_.acquireNextImage2KHR(vk::AcquireNextImageInfoKHR{
        .swapchain = current_swapchain_.swapchain.get(),
        .timeout = 1000000,
        .semaphore = {},
        .fence = {},
        .deviceMask = 0
    });

    if (res.result == vk::Result::eErrorOutOfDateKHR)
    {
	    return std::nullopt;
    }
    else if (res.result != vk::Result::eSuccess && res.result != vk::Result::eSuboptimalKHR)
    {
	    throw std::runtime_error("Swapchain lost!");
    }

    return SwapchainImage{current_swapchain_.elements[res.value].image_view.get(), vk::Semaphore{}, res.value};
}

bool WindowRenderer::present(vk::Semaphore wait, uint32_t index)
{
    auto res = present_queue_.presentKHR(vk::PresentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &wait,
            .swapchainCount = 1,
            .pSwapchains = &current_swapchain_.swapchain.get(),
            .pImageIndices = &index,
        });

    if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR)
    {
        return false;
    }
    else if (res != vk::Result::eSuccess)
    {
        throw std::runtime_error("Graphics queue submission failed!");
    }

    return true;
}

void WindowRenderer::recreateSwapchain()
{
    current_swapchain_ = createSwapchain();
}

WindowRenderer::SwapchainData WindowRenderer::createSwapchain() const
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

    SwapchainData new_swapchain;

    new_swapchain.swapchain = device_.createSwapchainKHRUnique(vk::SwapchainCreateInfoKHR{
            .surface = surface_.get(),
            .minImageCount = image_count,
            .imageFormat = format.format,
            .imageColorSpace = format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queue_family_,
            .preTransform = surface_caps.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = present_mode,
            .clipped = true,
            .oldSwapchain = current_swapchain_.swapchain.get()
        });

    new_swapchain.format = format.format;
    new_swapchain.extent = extent;

    auto imgs = device_.getSwapchainImagesKHR(new_swapchain.swapchain.get());
    
    new_swapchain.elements.resize(imgs.size());
    for (std::size_t i = 0; i < imgs.size(); ++i)
    {
        new_swapchain.elements[i].image = imgs[i];
        new_swapchain.elements[i].image_view = device_.createImageViewUnique(vk::ImageViewCreateInfo{
                .image = imgs[i],
                .viewType = vk::ImageViewType::e2D,
                .format = format.format,
                .components = vk::ComponentMapping{},
                .subresourceRange = vk::ImageSubresourceRange{
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
