#pragma once

#include <flecs.h>
#include <string_view>

#include "rendering/GlobalRenderer.hpp"


struct CGlobalRendererRef
{
    GlobalRenderer* ref = nullptr;
};

std::unique_ptr<GlobalRenderer> register_vulkan_systems(flecs::world& world, std::string_view app_name);
