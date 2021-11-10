#include "rendering/GlobalRenderer.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <unordered_set>

#include <spdlog/spdlog.h>
#include <unifex/just.hpp>
#include <unifex/when_all.hpp>

#include "util/Assert.hpp"
#include "core/EngineHandle.hpp"


constexpr std::array DEVICE_EXTENSIONS {static_cast<const char*>(VK_KHR_SWAPCHAIN_EXTENSION_NAME)};


QueueFamilyIndices chose_queue_families(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface)
{
    auto queues = device.getQueueFamilyProperties();

    constexpr uint32_t INVALID = std::numeric_limits<uint32_t>::max();
    QueueFamilyIndices result{INVALID, INVALID};

    uint32_t queue_idx = 0;
    for (const auto& queue : queues)
    {
        if (queue.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            result.graphics_queue_idx = queue_idx;
        }

        if (device.getSurfaceSupportKHR(queue_idx, surface))
        {
            result.presentation_queue_idx = queue_idx;
        }

        ++queue_idx;
    }

    NG_VERIFY(result.graphics_queue_idx != INVALID, "GPU does not support graphics!");
    NG_VERIFY(result.presentation_queue_idx != INVALID, "GPU does not support presentation!");

    return result;
}


GlobalRenderer::GlobalRenderer(vk::ApplicationInfo info, std::span<const char* const> layers, std::span<const char* const> extensions)
{
    instance_ = vk::createInstanceUnique(vk::InstanceCreateInfo{
            {}, &info,
            static_cast<uint32_t>(layers.size()), layers.data(),
            static_cast<uint32_t>(extensions.size()), extensions.data()
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
}

WindowRenderer* GlobalRenderer::makeWindowRenderer(vk::UniqueSurfaceKHR surface, ResolutionProvider resolution_provider)
{
    // Doing a device per window is pretty stupid cuz we can't share resources this way
    if (!device_)
    {
        initializeDevice(surface.get());
    }
    else
    {
        NG_VERIFY(
                physical_device_.getSurfaceSupportKHR(queueFamilyIndices_.presentation_queue_idx, surface.get()),
                "Some windows don't support the chosen present queue! Handling this is a royal pain in the ass!"
            );
    }

    window_renderers_.push_back(std::make_unique<WindowRenderer>(
            physical_device_, device_.get(), allocator_.get(), std::move(surface),
            WindowRenderer::RequiredQueues{queueFamilyIndices_.presentation_queue_idx, queueFamilyIndices_.graphics_queue_idx},
            std::move(resolution_provider)
        ));

    return window_renderers_.back().get();
}

void GlobalRenderer::initializeDevice(vk::SurfaceKHR surface)
{
    queueFamilyIndices_ = chose_queue_families(physical_device_, surface);

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    {
        // kostyl due to bad API design
        float queue_priority = 1.0f;

        std::unordered_set<uint32_t> unique_families{
            queueFamilyIndices_.graphics_queue_idx,
            queueFamilyIndices_.presentation_queue_idx
        };

        queue_create_infos.reserve(unique_families.size());
        for (auto idx : unique_families)
        {
            queue_create_infos.emplace_back(vk::DeviceQueueCreateInfo{{}, idx, 1, &queue_priority});
        }
    }


    vk::PhysicalDeviceFeatures features;

    // TODO: make this external and configurable somehow
    features.tessellationShader = true;
    features.geometryShader = true;

    device_ = physical_device_.createDeviceUnique(vk::DeviceCreateInfo{
        {},
        static_cast<uint32_t>(queue_create_infos.size()), queue_create_infos.data(),
        0, nullptr,
        static_cast<uint32_t>(DEVICE_EXTENSIONS.size()), DEVICE_EXTENSIONS.data(),
        &features
    });

    VmaAllocatorCreateInfo info{
        .flags = {},
        .physicalDevice = physical_device_,
        .device = device_.get(),

        .preferredLargeHeapBlockSize = {},
        .pAllocationCallbacks = {},
        .pDeviceMemoryCallbacks = {},
        .frameInUseCount = MAX_FRAMES_IN_FLIGHT,
        .pHeapSizeLimit = {},
        .pVulkanFunctions = {},
        .pRecordSettings = {},

        .instance = instance_.get(),
        .vulkanApiVersion = VK_API_VERSION_1_2, // TODO: global constant for this
    };

    VmaAllocator allocator;
    vmaCreateAllocator(&info, &allocator);

    allocator_ = {allocator, &vmaDestroyAllocator};
}

unifex::task<void> GlobalRenderer::renderFrame(float delta_seconds)
{
    co_await unifex::schedule(g_engine.main_scheduler());

    co_return;
}
