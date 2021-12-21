#pragma once

struct ImGuiContext;
extern thread_local ImGuiContext* NgImGui;
#define GImGui NgImGui
