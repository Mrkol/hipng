#include "rendering/RenderingSubsystem.hpp"

#include <unordered_set>

#include <spdlog/spdlog.h>
#include <unifex/just.hpp>
#include <unifex/when_all.hpp>
#include <unifex/on.hpp>

#include "core/EngineHandle.hpp"
#include "util/DebugBreak.hpp"
#include "util/Defer.hpp"


constexpr std::array DEVICE_EXTENSIONS {
	static_cast<const char*>(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
    static_cast<const char*>(VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME),
    static_cast<const char*>(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME),
};


RenderingSubsystem::RenderingSubsystem(CreateInfo info)
    : inflight_mutex_(std::in_place)
{
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

    {
        const auto chain = physical_device_.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceSynchronization2FeaturesKHR
            >();

        auto& features = chain.get<vk::PhysicalDeviceFeatures2>();
        NG_VERIFY(features.features.geometryShader && features.features.tessellationShader);

        auto& syncFeatures = chain.get<vk::PhysicalDeviceSynchronization2FeaturesKHR>();
        NG_VERIFY(syncFeatures.synchronization2);
    }


    // TODO: make this external and configurable somehow
    vk::PhysicalDeviceFeatures features {
            .geometryShader = true,
            .tessellationShader = true,
        };

    vk::PhysicalDeviceSynchronization2FeaturesKHR sync2_features{
        .synchronization2 = true
    };

    device_ = physical_device_.createDeviceUnique(vk::DeviceCreateInfo{
			.pNext = &sync2_features,
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
                .pHeapSizeLimit = {},
                .pVulkanFunctions = {},

                .instance = instance_.get(),
                .vulkanApiVersion = VK_API_VERSION_1_2, // TODO: global constant for this
            };

        VmaAllocator allocator;
        vmaCreateAllocator(&alloc_info, &allocator);

        allocator_ = {allocator, &vmaDestroyAllocator};
    }

    oneshot_.emplace(
			[this](std::size_t)
			{
				return device_->createCommandPoolUnique(vk::CommandPoolCreateInfo{
				        .flags = vk::CommandPoolCreateFlagBits::eTransient,
				        .queueFamilyIndex = graphics_queue_idx_,
				    });
			},
            [this](std::size_t)
            {
	            return device_->createFenceUnique(vk::FenceCreateInfo{});
            }
		);

    gpu_storage_manager_ = std::make_unique<GpuStorageManager>(
        GpuStorageManager::CreateInfo{
            .device = device_.get(),
            .allocator = allocator_.get(),
		});
}

unifex::task<void> RenderingSubsystem::makeVkWindow(vk::UniqueSurfaceKHR surface,
    ResolutionProvider resolution_provider, ImGuiContext* gui_context)
{
    NG_VERIFYF(
            physical_device_.getSurfaceSupportKHR(graphics_queue_idx_, surface.get()),
            "Some windows can't use the chosen graphics queue for presentation!"
        );

    auto window = std::make_unique<Window>(Window::CreateInfo{
            .physical_device = physical_device_,
            .device = device_.get(),
            .allocator = allocator_.get(),
            .surface = std::move(surface),
            .resolution_provider = std::move(resolution_provider),
			.present_queue = device_->getQueue(graphics_queue_idx_, 0),
            .queue_family = graphics_queue_idx_,
        });

    // TODO: REMOVE THIS KOSTYL
    auto renderer = std::make_unique<TempForwardRenderer>(
            TempForwardRenderer::CreateInfo{
				.instance = instance_.get(),
				.physical_device = physical_device_,
                .device = device_.get(),
				.allocator = allocator_.get(),
                .graphics_queue = device_->getQueue2(
                        vk::DeviceQueueInfo2{
                            .queueFamilyIndex = graphics_queue_idx_,
                            .queueIndex = 0
                        }),
                .queue_family = graphics_queue_idx_,
				.storage_manager = gpu_storage_manager_.get(),
				.gui_context = gui_context,
            });

    auto res = co_await window->recreateSwapchain();
    NG_VERIFY(res.has_value());
    auto imgs = window->getAllImages();
    co_await renderer->updatePresentationTarget(imgs, res.value());
    window->markSwapchainRecreated();

    
    
    co_await frame_mutex_.async_lock();
    Defer defer{[this]() { frame_mutex_.unlock(); }};

    window_renderer_mapping_.emplace(window.get(), renderer.get());
    windows_.emplace_back(std::move(window));
    renderers_.emplace_back(std::move(renderer));

    co_return;
}

VkBool32 RenderingSubsystem::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                           VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                           void* pUserData)
{
    auto& self = *static_cast<RenderingSubsystem*>(pUserData);

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        spdlog::error("Vulkan: {}", pCallbackData->pMessage);
        DEBUG_BREAK();
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
	    spdlog::warn("Vulkan: {}", pCallbackData->pMessage);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
	    spdlog::info("Vulkan: {}", pCallbackData->pMessage);
    }
    else
    {
	    spdlog::trace("Vulkan: {}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

unifex::task<void> RenderingSubsystem::renderFrame(std::size_t frame_index, FramePacket packet)
{
    auto& inflight_mtx = *inflight_mutex_.get(frame_index);
    Defer defer{[&inflight_mtx]() { inflight_mtx.unlock(); }};
    co_await inflight_mtx.async_lock();

    co_await frame_mutex_.async_lock(); // WARNING: this gets unlocked at a peculiar time, don't mess this up!!!

    // copy shared state
    std::vector<Window*> my_windows;
    my_windows.reserve(windows_.size());
    for (auto& window : windows_)
    {
	    my_windows.emplace_back(window.get());
    }

    std::vector<std::optional<Window::SwapchainImage>> window_images;
    window_images.reserve(my_windows.size());
    for (auto& window : my_windows)
    {
        window_images.push_back(co_await window->acquireNext(frame_index));
    }

    auto oneshot_pool = oneshot_->pool.get(frame_index)->get();

    Defer defer2{[this, frame_index, oneshot_pool]() { device_->resetCommandPool(oneshot_pool); }};

    auto cb = std::move(device_->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
        .commandPool = oneshot_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    })[0]);

    cb->begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    auto uploads_done = co_await gpu_storage_manager_->frameUpload(cb.get());
    cb->end();

    auto oneshot_fence = oneshot_->fence.get(frame_index)->get();
    
	device_->getQueue(graphics_queue_idx_, 0).submit(std::array{vk::SubmitInfo{
			.commandBufferCount = 1,
	        .pCommandBuffers = &cb.get(),
	    }}, oneshot_fence);

    
    

    // TODO: THIS IS A DUMB PROOF OF CONCEPT
    // needs to be alot more intricate than this
    std::vector<std::optional<IRenderer::RenderingDone>> renderings_done;
    renderings_done.reserve(window_images.size());
    for (std::size_t i = 0; i < window_images.size(); ++i)
    {
        if (window_images[i].has_value())
        {
            renderings_done.emplace_back(window_renderer_mapping_[my_windows[i]]
                ->render(frame_index, window_images[i]->view, window_images[i]->available, packet));
        }
        else
        {
            renderings_done.emplace_back(std::nullopt);
        }
    }


    for (std::size_t i = 0; i < my_windows.size(); ++i)
    {
        if (window_images[i].has_value())
        {
            if (!my_windows[i]->present(renderings_done[i].value().sem, window_images[i].value().view))
            {
                // TODO: This code is VERY BAD :(
                my_windows[i]->markImageFree(window_images[i].value().view);
                window_images[i] = std::nullopt;
            }
        }
    }

    frame_mutex_.unlock();





    // WARNING: none of the windows OR renderers for which a submission succeeded should get destroyed
    // before this function finishes. Right now we never destroy windows, but it CAN become a problem later on.
    // TODO: sensible timeout
    std::vector<vk::Fence> fences;
    fences.reserve(renderings_done.size());
    for (auto& sem_fence : renderings_done)
    {
        if (sem_fence.has_value())
        {
            fences.push_back(sem_fence.value().fence);
        }
    }
    
    fences.push_back(oneshot_fence);
    

    if (!fences.empty())
    {
        co_await unifex::schedule(g_engine.blockingScheduler());

        auto res = device_->waitForFences(fences, true, 1000000000);
        // TODO: if this fails, the device was probably lost, so something cleverer is needed here.
        NG_VERIFY(res == vk::Result::eSuccess);
        device_->resetFences(fences);

        co_await unifex::schedule(g_engine.mainScheduler());
    }

    gpu_storage_manager_->frameUploadDone(std::move(uploads_done));

    for (std::size_t i = 0; i < my_windows.size(); ++i)
    {
        if (window_images[i].has_value())
        {
            my_windows[i]->markImageFree(window_images[i].value().view);
        }
    }

    // If any swapchains were out of date or suboptimal, recreate them.
    for (std::size_t i = 0; i < my_windows.size(); ++i)
    {
        if (!window_images[i].has_value())
        {
            // This call is asynchronous, as it needs to wait for previous frames to finish presenting.
            auto res = co_await my_windows[i]->recreateSwapchain();
            if (!res.has_value())
            {
                continue;
            }
            g_engine.async(
                [](Window* window, IRenderer* renderer, vk::Extent2D resolution)
					-> unifex::task<void>
	            {
		            auto imgs = window->getAllImages();
		            co_await renderer->updatePresentationTarget(imgs, resolution);
		            window->markSwapchainRecreated();
		            co_return;
	            }(my_windows[i], window_renderer_mapping_[my_windows[i]], res.value()));
        }
    }

    co_return;
}
