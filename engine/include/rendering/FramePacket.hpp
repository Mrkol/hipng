#pragma once

#include <unordered_map>
#include <vector>

#include "assets/AssetHandle.hpp"
#include "rendering/gui/GuiFramePacket.hpp"
#include "shader_cpp_bridge/static_mesh.h"


struct ImGuiContext;

struct StaticMeshPacket
{
	glm::mat4x4 transform;
	AssetHandle model;
};

/**
 * Should have all the data required for a frame to be rendered
 */
struct FramePacket
{
	glm::mat4x4 view;
	float fov;
	float aspect; // HANDLED BY RENDERER
	float near;
	float far;
	std::vector<StaticMeshPacket> static_meshes;

	std::unordered_map<ImGuiContext*, GuiFramePacket> gui_packets;
};

struct CCurrentFramePacket
{
	FramePacket* packet;
};
