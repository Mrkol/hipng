#pragma once

#include <vector>
#include <memory>
#include <string_view>

#include <function2/function2.hpp>
#include <GLFW/glfw3.h>
#include <flecs.h>

#include "util/Mixins.hpp"
#include "unifex/thread_unsafe_event_loop.hpp"


struct CWindow
{
    std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> glfw_window{nullptr, &glfwDestroyWindow};
};

// When the main window is closed, the program quits. At least one main window is required.
struct TMainWindow {};

// This can be added to windows to make them usable for vulkan rendering
struct TRequiresVulkan {};


CWindow create_window(std::string_view name);

void register_window_systems(flecs::world& world);
