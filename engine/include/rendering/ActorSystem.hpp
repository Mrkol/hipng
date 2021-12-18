#pragma once

#include <flecs/flecs.hpp>

#include "assets/AssetHandle.hpp"


struct CStaticMeshActor
{
	AssetHandle model;
};

void register_actor_systems(flecs::world& world);
