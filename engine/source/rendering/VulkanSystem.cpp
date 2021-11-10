#include "VulkanSystem.hpp"

#include <spdlog/spdlog.h>

#include "core/DependencySystem.hpp"
#include "core/WindowSystem.hpp"
#include "rendering/WindowRenderer.hpp"
#include "util/Assert.hpp"


constexpr std::array VALIDATION_LAYERS {static_cast<const char*>("VK_LAYER_KHRONOS_validation")};

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

std::unique_ptr<GlobalRenderer> register_vulkan_systems(flecs::world& world, std::string_view app_name)
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

    auto result = std::make_unique<GlobalRenderer>(
            application_info,
            std::span{VALIDATION_LAYERS.begin(), VALIDATION_LAYERS.end()},
            std::span{glfwExts, glfwExts + glfwExtCount}
        );

    world.set<CGlobalRendererRef>({result.get()});


    world.observer<CWindow, const TRequiresVulkan>()
        .event(flecs::OnAdd)
        .each([](flecs::entity e, CWindow& window, const TRequiresVulkan&)
        {
            GlobalRenderer* renderer = e.world().get_mut<CGlobalRendererRef>()->ref;

            VkSurfaceKHR surface;
            auto res = glfwCreateWindowSurface(renderer->getInstance(), window.glfw_window.get(), nullptr, &surface);
            NG_VERIFY(res == VK_SUCCESS, "Unable to create VK surface!");

            auto unique_surface = vk::UniqueSurfaceKHR{
                    surface,
                    vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderStatic>{renderer->getInstance()}
                };
            (void) e.remove<TRequiresVulkan>()
                    .set<CWindowRendererRef>({
                        renderer->makeWindowRenderer(
                                std::move(unique_surface),
                                [window = window.glfw_window.get()]() mutable
                                {
                                    int width, height;
                                    glfwGetFramebufferSize(window, &width, &height);

                                    while (width == 0 || height == 0)
                                    {
                                        glfwWaitEvents();
                                        glfwGetFramebufferSize(window, &width, &height);
                                    }

                                    return vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
                                }
                            )
                    })
                    .add<RDependsOn>(e.world().component<CGlobalRendererRef>());
        });

    return result;
}
