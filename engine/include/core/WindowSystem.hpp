#pragma once

#include <memory>
#include <string_view>

#include <GLFW/glfw3.h>
#include <flecs.h>


struct CWindow
{
    std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> glfw_window{nullptr, &glfwDestroyWindow};
};

struct TMainWindow {};

// Tags used to dispatch window initialization to different subsystems
struct TRequiresVulkan {};
struct TRequiresGui {};

struct WindowCreateInfo
{
	std::string_view name;
	bool requiresVulkan;
	bool requiresImgui;
	bool mainWindow;
};

flecs::entity create_window(flecs::world& world, WindowCreateInfo info);

void register_window_systems(flecs::world& world);
