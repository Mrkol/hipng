#include "rendering/ActorSystem.hpp"

#include "core/GameplaySystem.hpp"


void register_actor_systems(flecs::world& world)
{
    world.system<CStaticMeshActor, CPosition>("Quit if main window closes")
		.kind(flecs::PostUpdate)
		.iter([](flecs::iter it, const CStaticMeshActor* actor, const CPosition* position)
		{
			
		});
}
