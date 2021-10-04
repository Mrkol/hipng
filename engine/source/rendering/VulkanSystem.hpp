#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "core/WindowSystem.hpp"


constexpr std::size_t MAX_FRAMES_IN_FLIGHT = 2;

struct CVkInstance
{
    vk::UniqueInstance instance;
};

struct CVkPhysicalDevice
{
    vk::PhysicalDevice physical_device;
};

struct CVkDevice
{
    vk::UniqueDevice device;
};

enum class QueueType { Graphics, Transfer, Present };

template<QueueType>
struct CVkQueue
{
    uint32_t idx;
    vk::Queue queue;
};

// This can be added to windows to create a surface and a swapchain for them
struct TRequiresVulkan {};

struct CKHRSurface
{
    vk::UniqueSurfaceKHR surface;
};

struct CVmaAllocator
{
    std::unique_ptr<std::remove_pointer_t<VmaAllocator>, void(*)(VmaAllocator)> allocator_{nullptr, nullptr};
};


void register_vulkan_systems(flecs::world& world, std::string_view app_name);
