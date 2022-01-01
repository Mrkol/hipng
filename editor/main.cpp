#include "core/Engine.hpp"
#include "core/EnginePhases.hpp"
#include "core/GameplaySystem.hpp"
#include "core/WindowSystem.hpp"
#include "rendering/ActorSystem.hpp"


int main(int argc, char** argv)
{
    Engine engine(argc, argv);

    engine.world().system<>()
        .kind(engine.world().component<TGameLoopStarting>())
        .iter([](flecs::iter it)
        {
            it.world().entity("MainWindow")
                .set<CWindow>(create_window("HipNg"))
                .add<TMainWindow>()
                .add<TRequiresVulkan>();
            
            /*it.world().entity("MainWindow2")
                .set<CWindow>(create_window("HipNg"))
                .add<TMainWindow>()
                .add<TRequiresVulkan>();*/
        });

    engine.world().system<CWindow>()
		.term<THasGui>()
		.each([](flecs::entity e, CWindow& window)
		{
			ImGui::SetCurrentContext(window.imgui_context.get());
            ImGui::ShowDemoWindow();
		});

    /*
    engine.world().system<CPosition, CStaticMeshActor>()
		.each([](flecs::entity e, CPosition& pos, CStaticMeshActor&)
		{
			auto win = e.world().entity("MainWindow2");
            if (win.has<THasGui>())
            {
	            
            }
		});*/

    return engine.run();
}
