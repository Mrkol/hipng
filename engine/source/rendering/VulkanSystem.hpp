#pragma once

#include <flecs.h>
#include <string_view>

#include "rendering/RenderingSubsystem.hpp"


struct CGlobalRendererRef
{
    RenderingSubsystem* ref = nullptr;
};

std::unique_ptr<RenderingSubsystem> register_vulkan_systems(flecs::world& world, std::string_view app_name);
