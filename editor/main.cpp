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
            (void) it.world().entity("MainWindow")
                .set<CWindow>(create_window("HipNg"))
                .add<TMainWindow>()
                .add<TRequiresVulkan>();
        });

    return engine.run();
}
