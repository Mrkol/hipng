#pragma once

#include <flecs.h>

#include "assets/AssetHandle.hpp"


struct CStaticMeshActor
{
	AssetHandle model;
	float scale;
};

struct CCameraActor
{
	float fovx;
	float fovy;
	float near;
	float far;
};

struct TActiveCamera{};

void register_actor_systems(flecs::world& world);
