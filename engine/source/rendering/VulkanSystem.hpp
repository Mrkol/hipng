#pragma once

#include <vulkan/vulkan.hpp>

#include "core/WindowSystem.hpp"


struct CVkInstance
{
    vk::UniqueInstance instance;
};

struct TRequiresVulkan {};

struct CKHRSurface
{
    vk::UniqueSurfaceKHR surface;
};

void register_vulkan_systems(flecs::world& world, std::string_view app_name);
