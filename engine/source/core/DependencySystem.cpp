#include "core/DependencySystem.hpp"

#include <spdlog/spdlog.h>

#include "core/EnginePhases.hpp"


void register_dependency_systems(flecs::world& world)
{
    world.component<RDependsOn>()
        .add(flecs::OnDeleteObject, flecs::Delete)
        .add(flecs::Transitive);

    world.system<>()
        .kind(world.component<TGameLoopFinished>())
        .term<RDependsOn>(flecs::Wildcard) // select all entities with (DependsOn, *)
        .term<RDependsOn>(flecs::Wildcard) // select from parent so we can do the cascade
            .super(world.component<RDependsOn>(), flecs::Cascade)
            .oper(flecs::Optional) // also match if parent doesn't have DependsOn
        .iter([](flecs::iter it)
        {
            for (auto i : it)
            {
                it.entity(i).destruct();
            }
        });
}
