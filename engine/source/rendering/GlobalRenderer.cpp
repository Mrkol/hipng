#include "rendering/GlobalRenderer.hpp"

#include <unordered_set>

#include <spdlog/spdlog.h>
#include <unifex/just.hpp>
#include <unifex/when_all.hpp>
#include <unifex/on.hpp>

#include "core/EngineHandle.hpp"
#include "util/DebugBreak.hpp"
#include "util/Defer.hpp"


constexpr std::array DEVICE_EXTENSIONS {static_cast<const char*>(VK_KHR_SWAPCHAIN_EXTENSION_NAME)};


GlobalRenderer::GlobalRenderer(GlobalRendererCreateInfo info)
    : frame_submitted_(std::in_place, false)
{
    // Game starts from frame 1, so to let it go through
    frame_submitted_.get(0)->set();

    instance_ = vk::createInstanceUnique(vk::InstanceCreateInfo{
            .pApplicationInfo = &info.app_info,
            .enabledLayerCount = static_cast<uint32_t>(info.layers.size()),
            .ppEnabledLayerNames = info.layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(info.extensions.size()),
            .ppEnabledExtensionNames = info.extensions.data()
        });
    
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_.get());

    auto pdevices = instance_->enumeratePhysicalDevices();

    auto best_device = pdevices.front();
    for (auto pdevice : pdevices)
    {
        auto props = pdevice.getProperties();
        if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
        {
            best_device = pdevice;
            break;
        }
    }

    spdlog::info("Choosing best GPU: {}", best_device.getProperties().deviceName);

    physical_device_ = best_device;

    graphics_queue_idx_ = findQueue(
            [](const vk::QueueFamilyProperties& queue) { return queue.queueFlags & vk::QueueFlagBits::eGraphics; });

    float queue_priority = 1.0f; // kostyl
    std::array queue_create_infos {
            vk::DeviceQueueCreateInfo{
                    .queueFamilyIndex = graphics_queue_idx_,
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority,
                }
        };


    // TODO: make this external and configurable somehow
    vk::PhysicalDeviceFeatures features {
            .geometryShader = true,
            .tessellationShader = true,
        };

    device_ = physical_device_.createDeviceUnique(vk::DeviceCreateInfo{
            .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
            .pQueueCreateInfos = queue_create_infos.data(),
            .enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size()),
            .ppEnabledExtensionNames = DEVICE_EXTENSIONS.data(),
            .pEnabledFeatures = &features,
        });

    VULKAN_HPP_DEFAULT_DISPATCHER.init(device_.get());

    
    //TODO: make this prettier
#ifndef NDEBUG
    debug_messenger_ = instance_->createDebugUtilsMessengerEXTUnique(
        vk::DebugUtilsMessengerCreateInfoEXT{
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
				| vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
				| vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
				| vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
				| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
				| vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            .pfnUserCallback = debugCallback,
            .pUserData = this
		});
#endif


    {
        VmaAllocatorCreateInfo alloc_info{
                .flags = {},
                .physicalDevice = physical_device_,
                .device = device_.get(),

                .preferredLargeHeapBlockSize = {},
                .pAllocationCallbacks = {},
                .pDeviceMemoryCallbacks = {},
                .frameInUseCount = static_cast<uint32_t>(g_engine.inflightFrames()),
                .pHeapSizeLimit = {},
                .pVulkanFunctions = {},
                .pRecordSettings = {},

                .instance = instance_.get(),
                .vulkanApiVersion = VK_API_VERSION_1_2, // TODO: global constant for this
            };

        VmaAllocator allocator;
        vmaCreateAllocator(&alloc_info, &allocator);

        allocator_ = {allocator, &vmaDestroyAllocator};
    }

    forward_renderer_ = std::make_unique<TempForwardRenderer>(
        TempForwardRenderer::CreateInfo{
            .device = device_.get(),
            .graphics_queue = device_->getQueue2(
                vk::DeviceQueueInfo2{
                	.queueFamilyIndex = graphics_queue_idx_,
                    .queueIndex = 0
                }),
        	.queue_family = graphics_queue_idx_,
        });
}

WindowRenderer* GlobalRenderer::makeWindowRenderer(vk::UniqueSurfaceKHR surface, ResolutionProvider resolution_provider)
{
    NG_VERIFYF(
            physical_device_.getSurfaceSupportKHR(graphics_queue_idx_, surface.get()),
            "Some windows can't use the chosen graphics queue for presentation!"
        );


    auto result = window_renderers_.emplace_back(std::make_unique<WindowRenderer>(WindowRenderer::CreateInfo{
            .physical_device = physical_device_,
            .device = device_.get(),
            .allocator = allocator_.get(),
            .surface = std::move(surface),
            .resolution_provider = std::move(resolution_provider),
			.present_queue = device_->getQueue(graphics_queue_idx_, 0),
            .queue_family = graphics_queue_idx_,
        })).get();

    // TODO: REMOVE THIS KOSTYL
    auto res = window_renderers_[0]->recreateSwapchain();
    auto imgs = window_renderers_[0]->getAllImages();
    forward_renderer_->updatePresentationTarget(imgs, res);

    return result;
}

VkBool32 GlobalRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
    auto& self = *static_cast<GlobalRenderer*>(pUserData);

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
	    spdlog::error(pCallbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
	    spdlog::warn(pCallbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
	    spdlog::info(pCallbackData->pMessage);
    }
    else
    {
	    spdlog::trace(pCallbackData->pMessage);
    }

    DEBUG_BREAK();

    return VK_FALSE;
}

unifex::task<void> GlobalRenderer::renderFrame(std::size_t frame_index)
{
    // TODO: remove temp kostyl
    auto recreateSwapchain =
        [this]()
        {
            // TODO: something sensible to make sure that all previous frames are done
            device_->getQueue(graphics_queue_idx_, 0).waitIdle();

            auto res = window_renderers_[0]->recreateSwapchain();
            auto imgs = window_renderers_[0]->getAllImages();
            forward_renderer_->updatePresentationTarget(imgs, res);
        };


    co_await unifex::on(g_engine.mainScheduler(), frame_submitted_.getPrevious(frame_index)->async_wait());
    frame_submitted_.getPrevious(frame_index)->reset();

    std::vector<std::optional<WindowRenderer::SwapchainImage>> window_images;
    window_images.reserve(window_renderers_.size());
    for (auto& window : window_renderers_)
    {
        window_images.push_back(co_await window->acquireNext(frame_index));
    }

    Defer defer(
        [this, &window_images, frame_index]() mutable
	    {
		    for (std::size_t i = 0; i < window_renderers_.size(); ++i)
		    {
		        if (window_images[i].has_value())
		        {
		            window_renderers_[i]->markImageFree(window_images[i].value().view);
		        }
		    }

		    frame_submitted_.get(frame_index)->set();
	    });
    
    
    // TODO: remove this kostyl
    NG_VERIFY(window_images.size() > 0);
    
    if (!window_images[0].has_value())
    {
        recreateSwapchain();
        co_return;
    }
    
    auto done = forward_renderer_->render(frame_index, window_images[0]->view, window_images[0]->available);


    for (std::size_t i = 0; i < window_renderers_.size(); ++i)
    {
        if (window_images[i].has_value())
        {
            if (!window_renderers_[i]->present(done.sem, window_images[i].value().view))
            {
                window_images[i] = std::nullopt;
            }
        }
    }

    co_await unifex::schedule(g_engine.blockingScheduler());
    // TODO: sensible timeout
    auto res = device_->waitForFences(std::array{done.fence}, true, 1000000000);
    // TODO: if this fails, the device was probably lost, so something cleverer is needed here.
	NG_VERIFY(res == vk::Result::eSuccess);

    device_->resetFences(std::array{done.fence});

    if (!window_images[0].has_value())
    {
        recreateSwapchain();
        co_return;
    }

    // No need to return to the main scheduler, as this coroutine ends soon

    co_return;
}
