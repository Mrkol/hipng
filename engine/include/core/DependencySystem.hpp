#pragma once

#include <flecs.h>


struct RDependsOn {};

void register_dependency_systems(flecs::world& world);
