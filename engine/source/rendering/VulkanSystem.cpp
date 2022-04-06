#include "VulkanSystem.hpp"

#include <unifex/let_value.hpp>
#include <unifex/sync_wait.hpp>

#include "core/DependencySystem.hpp"
#include "core/WindowSystem.hpp"
#include "rendering/Window.hpp"
#include "rendering/GuiSystem.hpp"
#include "util/Assert.hpp"


constexpr std::array VALIDATION_LAYERS {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};

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

std::unique_ptr<RenderingSubsystem> register_vulkan_systems(flecs::world& world, std::string_view app_name)
{
	VULKAN_HPP_DEFAULT_DISPATCHER.init(glfwGetInstanceProcAddress);
    
    check_validation_layers_support(VALIDATION_LAYERS);

    std::string name_copy(app_name);
    vk::ApplicationInfo application_info{
        .pApplicationName = name_copy.c_str(),
        .applicationVersion = 1u,
        .pEngineName = "Vulkan.hpp",
        .engineVersion = 1u,
        .apiVersion = VK_API_VERSION_1_2,
    };

    uint32_t glfwExtCount;
    auto glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);

    std::vector extensions(glfwExts, glfwExts + glfwExtCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    auto result = std::make_unique<RenderingSubsystem>(RenderingSubsystem::CreateInfo{
            .app_info = application_info,
            .layers = std::span{VALIDATION_LAYERS.begin(), VALIDATION_LAYERS.end()},
            .extensions = extensions,
        });

    world.set<CGlobalRendererRef>({result.get()});
    

    world.observer<CWindow, CUninitializedGui, const TRequiresVulkan>()
        .event(flecs::OnAdd)
        .each([](flecs::entity e, CWindow& window, CUninitializedGui& gui, const TRequiresVulkan&)
        {
            auto* kek = e.world().get_mut<CGlobalRendererRef>();
            RenderingSubsystem* renderingSystem = kek->ref;

            VkSurfaceKHR surface;
            auto res = glfwCreateWindowSurface(renderingSystem->getInstance(), window.glfw_window.get(), nullptr, &surface);
            NG_VERIFYF(res == VK_SUCCESS, "Unable to create VK surface!");

            auto unique_surface = vk::UniqueSurfaceKHR{
                    surface,
                    vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>{renderingSystem->getInstance()}
                };

            auto deferred_window_create =
                [] // NOLINT
                (flecs::entity entity, RenderingSubsystem* rendering_system,
                    GLFWwindow* window, GuiContextHolder context, vk::UniqueSurfaceKHR surface)
                    -> unifex::task<void>
                {
                    co_await unifex::schedule(g_engine.mainScheduler());

                    co_await rendering_system->makeVkWindow(
                            std::move(surface),
                            [window]() // NOLINT
                                    -> unifex::task<vk::Extent2D>
                            {
                                int width, height;
                                glfwGetFramebufferSize(window, &width, &height);

                                while (width == 0 || height == 0)
                                {
                                    co_await g_engine.scheduleOsPolling();
                                    glfwWaitEvents();
                                    glfwGetFramebufferSize(window, &width, &height);
                                }

                                co_return vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
                            },
							context.get()
                    );

                    if (context != nullptr)
                    {
	                    co_await unifex::schedule(g_engine.nextFrameScheduler());

	                    entity.set(CGui{ .context = std::move(context) });
                    }
                    
                    co_return;
                };

            (void) e.remove<TRequiresVulkan>();

            GuiContextHolder context = {nullptr, &ImGui::DestroyContext};

            // TODO: make GUI optional (flecs problem)
            //if (gui != nullptr)
            {
	            context = std::move(gui.context);
                e.remove<CUninitializedGui>();
            }

            g_engine.async(deferred_window_create(e, renderingSystem,
                window.glfw_window.get(), std::move(context), std::move(unique_surface)));

        });

    return result;
}
