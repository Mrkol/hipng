#pragma once

#include <memory>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <flecs.h>

#include "unifex/thread_unsafe_event_loop.hpp"


struct CWindow
{
    std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> glfw_window{nullptr, &glfwDestroyWindow};
    std::unique_ptr<ImGuiContext, void (*)(ImGuiContext*)> imgui_context{nullptr, &ImGui::DestroyContext};
};

// When the main window is closed, the program quits. At least one main window is required.
struct TMainWindow {};

// This can be added to windows to make them usable for vulkan rendering
struct TRequiresVulkan {};

// This is needed to defer GUI data loading. If we don't use this, imgui crashes
struct THasGui {};


CWindow create_window(std::string_view name);

void register_window_systems(flecs::world& world);
