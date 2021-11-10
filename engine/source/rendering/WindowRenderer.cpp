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

unifex::task<void> WindowRenderer::renderFrame(float delta_seconds)
{
    co_return;
}

void WindowRenderer::recreateSwapchain()
{
    device_.waitIdle();

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

    Swapchain swapchain;

    swapchain.swapchain = device_.createSwapchainKHRUnique(vk::SwapchainCreateInfoKHR{
            {}, surface_.get(),
            image_count, format.format, format.colorSpace,
            extent, 1, vk::ImageUsageFlagBits::eColorAttachment,
            queues_differ ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
            static_cast<uint32_t>(queue_families.size()), queue_families.data(),
            surface_caps.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque,
            present_mode,
            /* clipped */ true,
            /* old swapchain */ swapchain_.swapchain.get()
        });

    swapchain.format = format.format;
    swapchain.extent = extent;

    swapchain.depthbuffer = UniqueVmaImage(allocator_,
            DEPTHBUFFER_FORMAT, swapchain.extent, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment, VMA_MEMORY_USAGE_GPU_ONLY
        );

    swapchain.depthbuffer_view = device_.createImageViewUnique(vk::ImageViewCreateInfo{
            {}, swapchain.depthbuffer.get(), vk::ImageViewType::e2D, DEPTHBUFFER_FORMAT,
            vk::ComponentMapping{},
            vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}
        });


    std::array attachment_descriptions{
            vk::AttachmentDescription{
                    {}, swapchain.format, vk::SampleCountFlagBits::e1,
                    /* attachment ops */vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    /* stencil ops */ vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal
            },
            vk::AttachmentDescription{
                    {}, DEPTHBUFFER_FORMAT, vk::SampleCountFlagBits::e1,
                    /* attachment ops */ vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
                    /* stencil ops */ vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal
            }
        };

    vk::AttachmentReference color_attachment_reference{
            /* attachment */ 0,
            vk::ImageLayout::eColorAttachmentOptimal
        };

    vk::AttachmentReference depth_attachment_reference{
            /* attachment */ 1,
            vk::ImageLayout::eDepthStencilAttachmentOptimal
        };

    vk::SubpassDescription subpass_description{
            {}, vk::PipelineBindPoint::eGraphics,
            /* input */ 0, nullptr,
            /* attach count */ 1,
            /* color */ &color_attachment_reference,
            /* resolve */ nullptr,
            /* depth/stencil */ &depth_attachment_reference
        };

    std::array dependencies{
            vk::SubpassDependency{
                /* src */ VK_SUBPASS_EXTERNAL,
                /* dst */ 0,
                /* src stages */
                vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                /* dst stages */
                vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                /* src access */ {},
                /* dst access */ vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite
            }
        };

    swapchain.render_pass = device_.createRenderPassUnique(vk::RenderPassCreateInfo{
            {},
            static_cast<uint32_t>(attachment_descriptions.size()), attachment_descriptions.data(),
            1, &subpass_description,
            static_cast<uint32_t>(dependencies.size()), dependencies.data()
        });


    auto imgs = device_.getSwapchainImagesKHR(swapchain.swapchain.get());
    std::vector<SwapchainElement> elements{imgs.size()};
    {


        for (std::size_t i = 0; i < imgs.size(); ++i)
        {
            elements[i].image = imgs[i];
        }

        for (auto& el : elements)
        {
            el.image_view = device_.createImageViewUnique(vk::ImageViewCreateInfo{
                    {}, el.image, vk::ImageViewType::e2D, format.format,
                    vk::ComponentMapping{},
                    vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
            });

            std::array attachments{el.image_view.get(), swapchain.depthbuffer_view.get()};

            el.main_framebuffer = device_.createFramebufferUnique(vk::FramebufferCreateInfo{
                    {}, swapchain.render_pass.get(),
                    static_cast<uint32_t>(attachments.size()), attachments.data(),
                    swapchain.extent.width, swapchain.extent.height,
                    /* layers */ 1u
            });
        }


        for (auto& element : elements)
        {
            // as per https://developer.nvidia.com/blog/vulkan-dos-donts/
            element.command_pool = device_.createCommandPoolUnique(vk::CommandPoolCreateInfo{
                    {}, graphics_queue_.index
            });
        }

        for (auto& element : elements)
        {
            element.command_buffer =
                    std::move(device_.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
                            element.command_pool.get(),
                            vk::CommandBufferLevel::ePrimary, 1
                    }).front());
        }
    }
    swapchain.elements = std::move(elements);


    // TODO:
//            recreate_pipelines();
//
//            if (gui_ != nullptr)
//            {
//                create_gui_framebuffers();
//            }
}
