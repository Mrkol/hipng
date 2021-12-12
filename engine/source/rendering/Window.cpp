#include "rendering/Window.hpp"

#include <unifex/just.hpp>
#include <unifex/on.hpp>
#include "util/Assert.hpp"
#include "util/Defer.hpp"

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



Window::Window(CreateInfo info)
    : physical_device_{info.physical_device}
    , device_{info.device}
    , surface_(std::move(info.surface))
    , resolution_provider_{std::move(info.resolution_provider)}
    , queue_family_{info.queue_family}
    , present_queue_{info.present_queue}
	, image_available_sem_(
        [&info](std::size_t)
		{
			return info.device.createSemaphoreUnique(vk::SemaphoreCreateInfo{});
		})
{
}

auto Window::acquireNext(std::size_t frame_index)
	-> unifex::task<std::optional<SwapchainImage>>
{
    if (swapchain_missing_.load())
    {
        co_return std::nullopt;
    }

    auto sem = image_available_sem_.get(frame_index)->get();

    // This blocks on mobile when the swapchain has no available images,
    // therefore we try to avoid acquiring too many images by fine-tuning
    // inflight frames and swapchain size.
    uint32_t index;
    // non-throwing version
    auto res = device_.acquireNextImageKHR(
        current_swapchain_.swapchain.get(),
        100000000000,
        sem,
        {},
        &index
    );

    if (res == vk::Result::eErrorOutOfDateKHR)
    {
	    co_return std::nullopt;
    }

	if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR)
    {
        // Theoretically we could recover from this maybe?
	    throw std::runtime_error(std::string("Swapchain element acquisition failed! Error code ")
            + vk::to_string(res));
    }

    auto& element = current_swapchain_.elements[index];

    // Sometimes swapchain returns the same image twice in a row. In such a case, we can't do
    // inflight frames, this frame HAS to wait for the previous one to finish working with this
    // swapchain image.
    co_await element.mtx.async_lock();

    co_return SwapchainImage {
    		.view = element.image_view.get(),
    		.available = sem,
    	};
}

bool Window::present(vk::Semaphore wait, vk::ImageView which)
{
    auto index = viewToIdx(which);

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &wait,
        .swapchainCount = 1,
        .pSwapchains = &current_swapchain_.swapchain.get(),
        .pImageIndices = &index,
    };

    // non-throwing version
    auto res = present_queue_.presentKHR(&presentInfo);

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

std::vector<vk::ImageView> Window::getAllImages()
{
    std::vector<vk::ImageView> result;
    result.reserve(current_swapchain_.elements.size());
    for (auto& el : current_swapchain_.elements)
    {
	    result.push_back(el.image_view.get());
    }

    return result;
}

unifex::task<std::optional<vk::Extent2D>> Window::recreateSwapchain()
{
    if (swapchain_missing_.exchange(true))
    {
        // The previous frame is already recreating the swapchain.
        co_return std::nullopt;
    }

    for (auto& el : current_swapchain_.elements)
    {
        // Wait for all frames currently rendering to this window to finish,
        // new ones cannot start due to the swapchain_missing_ flag
        co_await el.mtx.async_lock();
    }

    // When the window gets minimized, we have to wait.
    auto resolution = co_await resolution_provider_();
    current_swapchain_ = createSwapchain(resolution);

    co_return current_swapchain_.extent;
}

void Window::markSwapchainRecreated()
{
    swapchain_missing_.store(false);
}

void Window::markImageFree(vk::ImageView which)
{
    current_swapchain_.elements[viewToIdx(which)].mtx.unlock();
}

Window::SwapchainData Window::createSwapchain(vk::Extent2D resolution) const
{
    auto surface_caps = physical_device_.getSurfaceCapabilitiesKHR(surface_.get());
    auto format = chose_surface_format(physical_device_, surface_.get());
    auto present_mode = chose_present_mode(physical_device_, surface_.get());
    auto extent = chose_swap_extent(surface_caps, resolution);

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
    
    new_swapchain.elements = HeapArray<SwapchainElement>(imgs.size());
    for (auto img : imgs)
    {
        auto& el = new_swapchain.elements.emplace_back();
        el.image = img;
        el.image_view = device_.createImageViewUnique(vk::ImageViewCreateInfo{
                .image = img,
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

uint32_t Window::viewToIdx(vk::ImageView view)
{
    uint32_t index = 0;
    while (index < current_swapchain_.elements.size()
        && current_swapchain_.elements[index].image_view.get() != view)
    {
	    ++index;
    }

    return index;
}
