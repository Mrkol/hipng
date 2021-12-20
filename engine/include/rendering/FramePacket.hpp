#pragma once

#include "assets/AssetHandle.hpp"
#include "shader_cpp_bridge/static_mesh.h"


struct StaticMeshPacket
{
	ObjectUBO ubo{};
	AssetHandle model;
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
