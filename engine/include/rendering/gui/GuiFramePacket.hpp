#pragma once

#include <imgui.h>


struct GuiFramePacket
{
	std::unique_ptr<ImDrawData, void(*)(ImDrawData*)> draw_data;
};
