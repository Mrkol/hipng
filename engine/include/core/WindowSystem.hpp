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

CWindow create_window(std::string_view name);

void register_window_systems(flecs::world& world);
