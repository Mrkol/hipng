#include "rendering/TempForwardRenderer.hpp"


TempForwardRenderer::TempForwardRenderer(CreateInfo info)
	: device_{info.device}
	, allocator_{info.allocator}
	, graphics_queue_{info.graphics_queue}
	, cb_pool_{
		[&info](std::size_t) -> vk::UniqueCommandPool
		{
			return info.device.createCommandPoolUnique(vk::CommandPoolCreateInfo{
				.queueFamilyIndex = info.queue_family
			});
		}}
	, storage_manager_{info.storage_manager}
	, gui_context_{info.gui_context}
	, rendering_done_fence_{
		[&info](std::size_t) -> vk::UniqueFence
		{
			return info.device.createFenceUnique(vk::FenceCreateInfo{});
		}}
	, rendering_done_sem_{
		[&info](std::size_t) -> vk::UniqueSemaphore
		{
			return info.device.createSemaphoreUnique(vk::SemaphoreCreateInfo{});
		}}
	, main_cb_(
		[this](std::size_t i)
		{
			return std::move(device_.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
					.commandPool = cb_pool_.get(i)->get(),
					.level = vk::CommandBufferLevel::ePrimary,
					.commandBufferCount = 1,
				})[0]);
		})
{
	std::array attachments {
		vk::AttachmentDescription2 {
			.format = vk::Format::eB8G8R8A8Srgb,
			.samples = vk::SampleCountFlagBits::e1,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.initialLayout = vk::ImageLayout::eUndefined,
			.finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
		},
		vk::AttachmentDescription2 {
			.format = vk::Format::eD32Sfloat,
			.samples = vk::SampleCountFlagBits::e1,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.initialLayout = vk::ImageLayout::eUndefined,
			.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
		},
	};
	

	std::array dependencies {
		vk::SubpassDependency2 {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			
			.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
			.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,

			.srcAccessMask = {},
			.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
		}
	};

	std::array attachment_refs {
		vk::AttachmentReference2 {
			.attachment = 0,
			.layout = vk::ImageLayout::eColorAttachmentOptimal,
			.aspectMask = vk::ImageAspectFlagBits::eColor
		}
	};
	
	vk::AttachmentReference2 depth_attachment_ref {
		.attachment = 1,
		.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
		.aspectMask = vk::ImageAspectFlagBits::eDepth
	};

	std::array subpasses {
		vk::SubpassDescription2 {
			.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
			.colorAttachmentCount = static_cast<uint32_t>(attachment_refs.size()),
			.pColorAttachments = attachment_refs.data(),
			.pDepthStencilAttachment = &depth_attachment_ref,
		}
	};

	renderpass_ = device_.createRenderPass2Unique(vk::RenderPassCreateInfo2{
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = static_cast<uint32_t>(subpasses.size()),
		.pSubpasses = subpasses.data(),
		.dependencyCount = static_cast<uint32_t>(dependencies.size()),
		.pDependencies = dependencies.data(),
	});

	std::array sizes{
		vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 100},
		vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 100},
		vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 100},
	};

	descriptor_pool_ = device_.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = 100,
		.poolSizeCount = static_cast<uint32_t>(sizes.size()),
		.pPoolSizes = sizes.data(),
	});

	static_mesh_renderer_ = std::make_unique<StaticMeshRenderer>(StaticMeshRenderer::CreateInfo{
		.device = device_,
		.allocator = allocator_,
		.storage_manager = storage_manager_,
		.ds_pool = descriptor_pool_.get(),
		.renderpass = renderpass_.get(),
	});

	gui_manager_recreate_info_ = GuiManager::CreateInfo{
            .instance = info.instance,
            .physical_device = info.physical_device,
            .device = info.device,
            .graphics_queue_idx = info.queue_family,
            .graphics_queue = graphics_queue_,
			.format = vk::Format::eB8G8R8A8Srgb,
			.context = gui_context_,
        };
}

TempForwardRenderer::RenderingDone TempForwardRenderer::render(std::size_t frame_index, vk::ImageView present_image,
	vk::Semaphore image_available, FramePacket& packet)
{
	packet.aspect = static_cast<float>(resolution_.width) / static_cast<float>(resolution_.height);

	device_.resetCommandPool(cb_pool_.get(frame_index)->get());

	auto cb = main_cb_.get(frame_index)->get();

	cb.begin(
		vk::CommandBufferBeginInfo{
			.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
		});
	{
        std::array clear_vals{
            vk::ClearValue{vk::ClearColorValue{std::array{0.3f, 0.3f, 0.3f, 1.f}}},
            vk::ClearValue{vk::ClearDepthStencilValue{1, 0}}
        };

		cb.beginRenderPass2(
			vk::RenderPassBeginInfo{
				.renderPass = renderpass_.get(),
				.framebuffer = framebuffer_.at(present_image).main.get(),
				.renderArea = vk::Rect2D{.offset = {0,0}, .extent = resolution_},
				.clearValueCount = static_cast<uint32_t>(clear_vals.size()),
				.pClearValues = clear_vals.data()
			},
			vk::SubpassBeginInfo{
				.contents = vk::SubpassContents::eInline
			});

		{
			vk::Viewport viewport{
				.width = static_cast<float>(resolution_.width),
				.height = static_cast<float>(resolution_.height),
				.minDepth = 0,
				.maxDepth = 1,
			};
			cb.setViewport(0, 1, &viewport);

			vk::Rect2D scissor{
				.extent = resolution_
			};

			cb.setScissor(0, 1, &scissor);
		}

		static_mesh_renderer_->render(frame_index, cb, packet);

		cb.endRenderPass2(vk::SubpassEndInfo{});


		
		
        std::array clear_vals2{
            vk::ClearValue{vk::ClearColorValue{std::array{0.f, 0.f, 0.f, 1.f}}},
        };
		cb.beginRenderPass2(
			vk::RenderPassBeginInfo{
				.renderPass = gui_manager_->getRenderPass(),
				.framebuffer = framebuffer_.at(present_image).gui.get(),
				.renderArea = vk::Rect2D{.offset = {0,0}, .extent = resolution_},
				.clearValueCount = static_cast<uint32_t>(clear_vals2.size()),
				.pClearValues = clear_vals2.data(),
			},
			vk::SubpassBeginInfo{
				.contents = vk::SubpassContents::eInline
			});

		if (packet.gui_packets.contains(gui_context_))
		{
			gui_manager_->render(cb, packet.gui_packets.at(gui_context_));
		}

		cb.endRenderPass2(vk::SubpassEndInfo{});
	}
	cb.end();

	vk::PipelineStageFlags waitMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	auto sem = rendering_done_sem_.get(frame_index)->get();
	auto fence = rendering_done_fence_.get(frame_index)->get();

	vk::SubmitInfo submit{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &image_available,
		.pWaitDstStageMask = &waitMask,
		.commandBufferCount = 1,
		.pCommandBuffers = &cb,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &sem,
	};

	NG_VERIFY(graphics_queue_.submit(1, &submit, fence) == vk::Result::eSuccess);


	return {sem, fence};
}

void TempForwardRenderer::updatePresentationTarget(std::span<vk::ImageView> target, vk::Extent2D resolution)
{
	resolution_ = resolution;
	framebuffer_.clear();

	gui_manager_recreate_info_.swapchain_size = target.size();
	gui_manager_.reset();
    gui_manager_ = std::make_unique<GuiManager>(gui_manager_recreate_info_);
	storage_manager_->uploadGuiData(gui_context_);
	
	
	{
		depth_buffer_ = UniqueVmaImage(allocator_, vk::Format::eD32Sfloat,
			resolution, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, 
			VMA_MEMORY_USAGE_GPU_ONLY);
		depth_buffer_view_ = device_.createImageViewUnique(vk::ImageViewCreateInfo{
			.image = depth_buffer_.get(),
			.viewType = vk::ImageViewType::e2D,
			.format = vk::Format::eD32Sfloat,
			.components = vk::ComponentMapping{},
			.subresourceRange = vk::ImageSubresourceRange{
				.aspectMask = vk::ImageAspectFlagBits::eDepth,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		});
	}

	for (auto image : target)
	{
		std::array attachments{image, depth_buffer_view_.get()};
		framebuffer_.insert(std::make_pair(static_cast<VkImageView>(image),
			Framebuffers{
				device_.createFramebufferUnique(
					vk::FramebufferCreateInfo{
						.renderPass = renderpass_.get(),
						.attachmentCount = static_cast<uint32_t>(attachments.size()),
						.pAttachments = attachments.data(),
						.width = resolution_.width,
						.height = resolution_.height,
						.layers = 1,
					}),
				device_.createFramebufferUnique(
					vk::FramebufferCreateInfo{
						.renderPass = gui_manager_->getRenderPass(),
						.attachmentCount = 1,
						.pAttachments = &image,
						.width = resolution_.width,
						.height = resolution_.height,
						.layers = 1,
					})
				}
			));
	}
}
