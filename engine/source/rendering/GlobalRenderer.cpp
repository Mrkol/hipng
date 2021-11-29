#include "rendering/GlobalRenderer.hpp"

#include <unordered_set>

#include <spdlog/spdlog.h>
#include <unifex/just.hpp>
#include <unifex/when_all.hpp>
#include <unifex/on.hpp>

#include "core/EngineHandle.hpp"


constexpr std::array DEVICE_EXTENSIONS {static_cast<const char*>(VK_KHR_SWAPCHAIN_EXTENSION_NAME)};


GlobalRenderer::GlobalRenderer(GlobalRendererCreateInfo info)
    : max_frames_in_flight_{info.max_frames_in_flight}
    , frame_submitted_events_(max_frames_in_flight_)
{
    while (frame_submitted_events_.size() < max_frames_in_flight_)
    {
        frame_submitted_events_.emplace_back(false);
    }
    // let the 0th frame through
    frame_submitted_events_.back().set();

    instance_ = vk::createInstanceUnique(vk::InstanceCreateInfo{
            .pApplicationInfo = &info.app_info,
            .enabledLayerCount = static_cast<uint32_t>(info.layers.size()),
            .ppEnabledLayerNames = info.layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(info.extensions.size()),
            .ppEnabledExtensionNames = info.extensions.data()
        });

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

    {
        VmaAllocatorCreateInfo alloc_info{
                .flags = {},
                .physicalDevice = physical_device_,
                .device = device_.get(),

                .preferredLargeHeapBlockSize = {},
                .pAllocationCallbacks = {},
                .pDeviceMemoryCallbacks = {},
                .frameInUseCount = max_frames_in_flight_,
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

    inflight_fences_.reserve(max_frames_in_flight_);
    while (inflight_fences_.size() < max_frames_in_flight_)
    {
        inflight_fences_.push_back(device_->createFenceUnique(vk::FenceCreateInfo{}));
    }
}

WindowRenderer* GlobalRenderer::makeWindowRenderer(vk::UniqueSurfaceKHR surface, ResolutionProvider resolution_provider)
{
    NG_VERIFYF(
            physical_device_.getSurfaceSupportKHR(graphics_queue_idx_, surface.get()),
            "Some windows can't use the chosen graphics queue for presentation!"
        );


    return window_renderers_.emplace_back(std::make_unique<WindowRenderer>(WindowRendererCreateInfo{
            .physical_device = physical_device_,
            .device = device_.get(),
            .allocator = allocator_.get(),
            .surface = std::move(surface),
            .resolution_provider = std::move(resolution_provider),
            .queue_family = graphics_queue_idx_,
        })).get();
}

unifex::task<void> GlobalRenderer::renderFrame(std::size_t frame_index)
{
    std::size_t this_inflight_index = frame_index % max_frames_in_flight_;
    std::size_t prev_inflight_index = (max_frames_in_flight_ + frame_index - 1) % max_frames_in_flight_;

    co_await unifex::on(g_engine.main_scheduler(), frame_submitted_events_[prev_inflight_index].async_wait());
    frame_submitted_events_[this_inflight_index].reset();

    std::vector<std::optional<WindowRenderer::SwapchainImage>> window_images;
    window_images.reserve(window_renderers_.size());
    for (auto& window : window_renderers_)
    {
        window_images.push_back(window->acquireNext());
    }

    std::vector<vk::Semaphore> waitSems;
    for (auto& img : window_images)
    {
        if (img.has_value())
        {
            waitSems.push_back(img.value().available);
        }
    }



    for (std::size_t i = 0; i < window_renderers_.size(); ++i)
    {
        if (window_images[i].has_value())
        {
            auto& image = window_images[i].value();
            window_renderers_[i]->present(image.available, image.index);
        }
    }

    frame_submitted_events_[frame_index].set();

    co_return;
}
