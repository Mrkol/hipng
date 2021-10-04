#include "VulkanSystem.hpp"

#include "core/Assert.hpp"
#include "core/DependencySystem.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <unordered_set>
#include <spdlog/spdlog.h>


constexpr std::array VALIDATION_LAYERS {static_cast<const char*>("VK_LAYER_KHRONOS_validation")};

constexpr std::array DEVICE_EXTENSIONS {static_cast<const char*>(VK_KHR_SWAPCHAIN_EXTENSION_NAME)};



template<std::size_t size>
void check_validation_layers_support(const std::array<const char*, size>& validation_layers)
{
    if constexpr (size != 0)
    {
        auto layers = vk::enumerateInstanceLayerProperties();
        for (auto valid_layer : validation_layers)
        {
            bool found = false;
            for (auto layer : layers)
            {
                if (std::string_view(layer.layerName) == valid_layer)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                throw std::runtime_error("Validation layer " + std::string(valid_layer) + " not supported!");
            }
        }
    }
}

struct QueueFamilyIndices
{
    uint32_t graphics_queue_idx{};
    uint32_t presentation_queue_idx{};
};

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

        if (device.getSurfaceSupportKHR(queue_idx , surface))
        {
            result.presentation_queue_idx = queue_idx;
        }

        ++queue_idx;
    }

    NG_VERIFY(result.graphics_queue_idx != INVALID, "GPU does not support graphics!");
    NG_VERIFY(result.presentation_queue_idx != INVALID, "GPU does not support presentation!");

    return result;
}

void register_vulkan_systems(flecs::world& world, std::string_view app_name)
{
    check_validation_layers_support(VALIDATION_LAYERS);

    std::string name_copy(app_name);
    vk::ApplicationInfo application_info{
        name_copy.c_str(), 1u,
        "Vulkan.hpp", 1u,
        VK_API_VERSION_1_2
    };

    uint32_t glfwExtCount;
    auto glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    
    world.set<CVkInstance>(
        {vk::createInstanceUnique(vk::InstanceCreateInfo{
            {}, &application_info,
            static_cast<uint32_t>(VALIDATION_LAYERS.size()), VALIDATION_LAYERS.data(),
            glfwExtCount, glfwExts
        })});



    
    {
        auto& instance = world.get<CVkInstance>()->instance;
        auto pdevices = instance->enumeratePhysicalDevices();

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
        
        world.set<CVkPhysicalDevice>({best_device});
    }
    
    world.observer<CWindow, const TRequiresVulkan>()
        .event(flecs::OnAdd)
        .each([&world](flecs::entity e, CWindow& window, const TRequiresVulkan&)
        {
            auto vulkan_instance = world.get<CVkInstance>()->instance.get();
            VkSurfaceKHR surface;
            auto res = glfwCreateWindowSurface(VkInstance(vulkan_instance), window.glfw_window.get(), nullptr, &surface);
            NG_VERIFY(res == VK_SUCCESS, "Unable to create VK surface!");
            e.remove<TRequiresVulkan>()
                .set<CKHRSurface>({vk::UniqueSurfaceKHR{surface,
                vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderStatic>{vulkan_instance}}})
                .add<RDependsOn>(world.component<CVkInstance>());
            
            auto physical_device = world.get<CVkPhysicalDevice>()->physical_device;

            vk::Device device;
            {
                auto chosen_queues = chose_queue_families(physical_device, surface);    

                std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
                {
                    // kostyl due to bad API design
                    float queue_priority = 1.0f;

                    std::unordered_set<uint32_t> unique_families{
                        chosen_queues.graphics_queue_idx,
                        chosen_queues.presentation_queue_idx
                    };

                    queue_create_infos.reserve(unique_families.size());
                    for (auto idx : unique_families)
                    {
                        queue_create_infos.emplace_back(vk::DeviceQueueCreateInfo{{}, idx, 1, &queue_priority});
                    }
                }


                vk::PhysicalDeviceFeatures features;

                features.tessellationShader = true;
                features.geometryShader = true;

                // NOTE: newer vulkan implementations ignore the layers here
                auto unique_device = physical_device.createDeviceUnique(vk::DeviceCreateInfo{
                    {},
                    static_cast<uint32_t>(queue_create_infos.size()), queue_create_infos.data(),
                    static_cast<uint32_t>(VALIDATION_LAYERS.size()), VALIDATION_LAYERS.data(),
                    static_cast<uint32_t>(DEVICE_EXTENSIONS.size()), DEVICE_EXTENSIONS.data(),
                    &features
                });

                device = unique_device.get();

                e.set<CVkDevice>({std::move(unique_device)});
                
                e.set<CVkQueue<QueueType::Graphics>>(
                    {chosen_queues.graphics_queue_idx, device.getQueue(chosen_queues.graphics_queue_idx, 0)});
                e.set<CVkQueue<QueueType::Present>>(
                    {chosen_queues.presentation_queue_idx, device.getQueue(chosen_queues.presentation_queue_idx, 0)});
            }
            
            {
                VmaAllocatorCreateInfo info{
                    .flags = {},
                    .physicalDevice = physical_device,
                    .device = device,

                    .preferredLargeHeapBlockSize = {},
                    .pAllocationCallbacks = {},
                    .pDeviceMemoryCallbacks = {},
                    .frameInUseCount = MAX_FRAMES_IN_FLIGHT,
                    .pHeapSizeLimit = {},
                    .pVulkanFunctions = {},
                    .pRecordSettings = {},

                    .instance = vulkan_instance,
                    .vulkanApiVersion = VK_API_VERSION_1_2, // TODO: global constant for this
                };

                VmaAllocator allocator;
                vmaCreateAllocator(&info, &allocator);

                e.set<CVmaAllocator>(
                    {decltype(CVmaAllocator::allocator_)(allocator, &vmaDestroyAllocator)}
                );
            }
        });
}
