#include "core/Engine.hpp"

#include <cxxopts.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/then.hpp>
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#include "core/Assert.hpp"
#include "core/EnginePhases.hpp"
#include "core/DependencySystem.hpp"
#include "core/WindowSystem.hpp"
#include "rendering/VulkanSystem.hpp"



EngineBase::EngineBase()
{
    auto retcode = glfwInit();
    NG_VERIFY(retcode == GLFW_TRUE, "Unable to initialize GLFW!");
}

EngineBase::~EngineBase()
{
    glfwTerminate();
}

Engine::Engine(int argc, char** argv)
    : last_tick_(Clock::now())
{
    cxxopts::Options options(APP_NAME, "The hip and cool engine");
    options.allow_unrecognised_options();

    auto parsed_opts = options.parse(argc, argv);

    register_dependency_systems(world_);
    register_vulkan_systems(world_, APP_NAME);
    register_window_systems(world_);

    main_window_ = world_.entity("MainWindow")
        .set<CWindow>(create_window("HipNg"))
        .add<TMainWindow>()
        .add<TRequiresVulkan>();
}


int Engine::run()
{
    return game_loop();
}

int Engine::game_loop()
{
    spdlog::info("Game loop starting");

    run_all(query_for_tag<TGameLoopStarting>(world_));

    while (true)
    {
        // Win32 does forces us to poll events from the same thread that has created the window
        glfwPollEvents();

        auto this_tick = Clock::now();
        float delta_seconds =
            std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1>>>(this_tick - last_tick_).count();
        last_tick_ = this_tick;
        
        if (!world_.progress(delta_seconds))
        {
            break;
        }
    }
    
    run_all(query_for_tag<TGameLoopFinished>(world_));

    spdlog::info("Game loop finished successfully");

    return 0;
}
