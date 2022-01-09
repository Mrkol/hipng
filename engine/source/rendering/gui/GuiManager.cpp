#include "rendering/gui/GuiManager.hpp"

#include <backends/imgui_impl_vulkan.h>

#include "rendering/gui/GuiFramePacket.hpp"
#include "util/Assert.hpp"


GuiManager::GuiManager(CreateInfo info)
	: context_{info.context}
{
	
    std::array pool_sizes{
        vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eInputAttachment, 1000},
    };
    descriptor_pool_ = info.device.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
        .maxSets =  1000 * static_cast<uint32_t>(pool_sizes.size()),
        .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
    	.pPoolSizes = pool_sizes.data(),
    });

    std::array attachment_descriptions{
        vk::AttachmentDescription{
            .format = info.format,
        	.samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eLoad,
        	.storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        	.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
        	.finalLayout = vk::ImageLayout::ePresentSrcKHR,
        }
    };

    vk::AttachmentReference color_attachment_reference{
        .attachment = 0,
        .layout = vk::ImageLayout::eColorAttachmentOptimal
    };

    vk::SubpassDescription subpass_description{
        .pipelineBindPoint =  vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
    	.pColorAttachments = &color_attachment_reference,
    };

    vk::SubpassDependency dependency{
    	.srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
    };

    render_pass_ = info.device.createRenderPassUnique(vk::RenderPassCreateInfo{
        .attachmentCount = static_cast<uint32_t>(attachment_descriptions.size()),
    	.pAttachments = attachment_descriptions.data(),
        .subpassCount = 1,
    	.pSubpasses = &subpass_description,
        .dependencyCount = 1,
    	.pDependencies = &dependency
    });


    if (context_ != nullptr)
    {
	    ImGui::SetCurrentContext(context_);
	    ImGui_ImplVulkan_InitInfo init_info{
	        .Instance = info.instance,
	        .PhysicalDevice = info.physical_device,
	        .Device = info.device,
	        .QueueFamily = info.graphics_queue_idx,
	        .Queue = info.graphics_queue,
	        .PipelineCache = VK_NULL_HANDLE,
	        .DescriptorPool = descriptor_pool_.get(),
	        .MinImageCount = 2,
	        .ImageCount = static_cast<uint32_t>(info.swapchain_size),
	        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
	        .Allocator = nullptr,
	        .CheckVkResultFn = [](VkResult res)
	        {
	            NG_VERIFY(res == VK_SUCCESS);
	        },
	    };

	    ImGui_ImplVulkan_Init(&init_info, render_pass_.get());
    }
}

void GuiManager::render(vk::CommandBuffer cb, GuiFramePacket& packet)
{
    ImGui_ImplVulkan_RenderDrawData(packet.draw_data.get(), cb);
}

GuiManager::~GuiManager()
{
    if (context_ != nullptr)
    {
	    ImGui::SetCurrentContext(context_);
	    ImGui_ImplVulkan_Shutdown();
    }
}
