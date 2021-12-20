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
	glm::mat4x4 view;
	float fov;
	float aspect; // HANDLED BY RENDERER
	float near;
	float far;
	std::vector<StaticMeshPacket> static_meshes;
};

struct CCurrentFramePacket
{
	FramePacket* packet;
};
