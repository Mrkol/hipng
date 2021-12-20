#include "rendering/ActorSystem.hpp"

#include "core/GameplaySystem.hpp"
#include <glm/mat4x4.hpp>

#include "rendering/FramePacket.hpp"
#include "util/Assert.hpp"
#include "util/DebugBreak.hpp"


void register_actor_systems(flecs::world& world)
{
    world.system<CStaticMeshActor, CPosition>("Send static meshes to rendering")
		.kind(flecs::PostUpdate)
		.iter([](flecs::iter it, const CStaticMeshActor* actor, const CPosition* position)
		{
			FramePacket* packet =
				it.world().component<CCurrentFramePacket>().get<CCurrentFramePacket>()->packet;
			NG_ASSERT(packet != nullptr);

			for (auto i : it)
			{
				auto mat = actor[i].scale * mat4_cast(position[i].rotation) *
					translate(glm::identity<glm::mat4x4>(), position[i].position);

				packet->static_meshes.emplace_back(StaticMeshPacket{
					.ubo = ObjectUBO{
						.model = mat,
						.normal = transpose(inverse(mat)),
					},
					.model = actor->model,
				});
			}
		});

    world.system<CCameraActor, CPosition>("Send camera to rendering")
		.kind(flecs::PostUpdate)
		.term<TActiveCamera>()
		.iter([](flecs::iter it, const CCameraActor* actor, const CPosition* position)
		{
			FramePacket* packet =
				it.world().component<CCurrentFramePacket>().get<CCurrentFramePacket>()->packet;
			NG_ASSERT(packet != nullptr);

			auto i = *it.begin();

			packet->view = mat4_cast(position[i].rotation) *
				translate(glm::identity<glm::mat4x4>(), position[i].position);
			packet->fov = actor[i].fov; 
			packet->near = actor[i].near; 
			packet->far = actor[i].far;
		});
}
