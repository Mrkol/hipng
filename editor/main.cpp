#include "core/Engine.hpp"
#include "core/EnginePhases.hpp"
#include "core/GameplaySystem.hpp"
#include "core/WindowSystem.hpp"
#include "rendering/GuiSystem.hpp"
#include "rendering/ActorSystem.hpp"


int main(int argc, char** argv)
{
    Engine engine(argc, argv);


    g_engine.async(unifex::then(unifex::schedule(g_engine.nextFrameScheduler()),
		[]()
		{
		    create_window(g_engine.world(), WindowCreateInfo{
		            .name = "HipNg",
		            .requiresVulkan = true,
		            .requiresImgui = true,
		            .mainWindow = true,
		        });
		}));

    engine.world().system<>()
        .kind(engine.world().component<TGameLoopStarting>())
        .iter([](flecs::iter it)
        {
            
            /*it.world().entity("MainWindow2")
                .set<CWindow>(create_window("HipNg"))
                .add<TMainWindow>()
                .add<TRequiresVulkan>();*/
        });

    engine.world().system<CGui>()
		.each([](flecs::entity e, CGui& gui)
		{
			ImGui::SetCurrentContext(gui.context.get());
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
