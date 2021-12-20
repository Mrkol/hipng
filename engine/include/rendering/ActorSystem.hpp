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
	float fov;
	float near;
	float far;
};

struct TActiveCamera{};

void register_actor_systems(flecs::world& world);
