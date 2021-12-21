#include "core/Engine.hpp"
#include "core/EnginePhases.hpp"
#include "core/WindowSystem.hpp"


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
		.each([](CWindow& window)
		{
			ImGui::SetCurrentContext(window.imgui_context.get());
            ImGui::ShowDemoWindow();
		});

    return engine.run();
}
