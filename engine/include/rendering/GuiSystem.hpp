#pragma once

#include <memory>
#include <backends/imgui_impl_glfw.h>
#include <flecs.h>


using GuiContextHolder = std::unique_ptr<ImGuiContext, void (*)(ImGuiContext*)>;

struct CUninitializedGui
{
    GuiContextHolder context{nullptr, &ImGui::DestroyContext};
};

struct CGui
{
    GuiContextHolder context{nullptr, &ImGui::DestroyContext};
};

void register_gui_systems(flecs::world& world);
