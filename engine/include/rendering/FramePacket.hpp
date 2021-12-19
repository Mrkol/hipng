#pragma once

#include <filesystem>

#include "shader_cpp_bridge/static_mesh.h"


struct StaticMeshPacket
{
	ObjectUBO ubo{};
	std::filesystem::path model;
};

/**
 * Should have all the data required for a frame to be rendered
 */
struct FramePacket
{
	GlobalUBO ubo{};
	std::vector<StaticMeshPacket> static_meshes;
};

struct CCurrentFramePacket
{
	FramePacket* packet;
};
