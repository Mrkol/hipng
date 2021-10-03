#include "VulkanSystem.hpp"

#include "core/Assert.hpp"
#include "core/DependencySystem.hpp"


constexpr std::array VALIDATION_LAYERS {"VK_LAYER_KHRONOS_validation",};

constexpr std::array EXTENSIONS {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_EXT_debug_utils"};



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
        });
}
