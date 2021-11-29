#include "core/Engine.hpp"

#include <queue>

#include <cxxopts.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/on.hpp>
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#include "concurrency/StaticScope.hpp"
#include "util/Assert.hpp"
#include "core/EnginePhases.hpp"
#include "core/DependencySystem.hpp"
#include "core/WindowSystem.hpp"
#include "rendering/VulkanSystem.hpp"



EngineHandle g_engine{nullptr};

EngineBase::EngineBase()
{
    auto retcode = glfwInit();
    NG_VERIFYF(retcode == GLFW_TRUE, "Unable to initialize GLFW!");
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

    g_engine = EngineHandle(this);

    register_dependency_systems(world_);
    renderer_ = register_vulkan_systems(world_, APP_NAME);
    register_window_systems(world_);
}

int Engine::run()
{
    return unifex::sync_wait(main_event_loop()).value_or(-1);
}

unifex::task<int> Engine::main_event_loop()
{
    co_await unifex::schedule(g_engine.main_scheduler());

    spdlog::info("Game loop starting");

    // We have to poll GLFW on the same thread the window got created :/
    run_all(query_for_tag<TGameLoopStarting>(world_));
    auto reschedule = unifex::schedule_with_subscheduler(g_engine.main_scheduler());

    auto render_windows = world_.query<CWindow>();

    bool should_quit = false;

    StaticScope<MAX_INFLIGHT_FRAMES, unifex::task<void>> rendering_scope(MAX_INFLIGHT_FRAMES);
    while (!should_quit)
    {
        co_await reschedule;
        glfwPollEvents();

        auto this_tick = Clock::now();
        float delta_seconds =
            std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1>>>(this_tick - last_tick_).count();
        last_tick_ = this_tick;
        
        should_quit |= !world_.progress(delta_seconds);

        co_await rendering_scope.spawn_next(renderer_->renderFrame(delta_seconds));
    }

    run_all(query_for_tag<TGameLoopFinished>(world_));
    
    main_thread_pool_.request_stop();
    blocking_thread_pool_.request_stop();

    spdlog::info("Game loop finished successfully");

    co_return 0;
}
