#include "core/Engine.hpp"

#include <queue>

#include <cxxopts.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/async_scope.hpp>
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#include "util/Assert.hpp"
#include "core/EnginePhases.hpp"
#include "core/DependencySystem.hpp"
#include "core/WindowSystem.hpp"
#include "rendering/VulkanSystem.hpp"
#include "rendering/WindowRenderer.hpp"


// Amount of frames the main thread is allowed to "outrun" the rendering
// 0 means sequential single-threaded-like rendering (but different windows will still get different rendering threads)
// 1 means concurrent rendering of previous frame and processing of the current one
// >2 means that several frames can be rendered in parallel
constexpr std::size_t RENDERING_FRAME_LATENCY = 1;

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
    , workers_(std::thread::hardware_concurrency() - 1)
{
    cxxopts::Options options(APP_NAME, "The hip and cool engine");
    options.allow_unrecognised_options();

    auto parsed_opts = options.parse(argc, argv);

    register_dependency_systems(world_);
    renderer_ = register_vulkan_systems(world_, APP_NAME);
    register_window_systems(world_);
}

int Engine::run()
{
    return main_event_loop();
}

int Engine::main_event_loop()
{
    spdlog::info("Game loop starting");

    run_all(query_for_tag<TGameLoopStarting>(world_));

    auto render_windows = world_.query<CWindow>();

    using RenderingJob = decltype(std::declval<unifex::async_scope>().cleanup());

    std::queue<RenderingJob> prev_frame_rendering_jobs;

    bool should_quit = false;
    while (!should_quit)
    {
        glfwPollEvents();

        auto this_tick = Clock::now();
        float delta_seconds =
            std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1>>>(this_tick - last_tick_).count();
        last_tick_ = this_tick;

        // "next frame" executor
        main_loop_events_.sync_wait(unifex::just());

        should_quit |= !world_.progress(delta_seconds);



        if constexpr (RENDERING_FRAME_LATENCY > 0) // NOLINT
        {
            while (prev_frame_rendering_jobs.size() >= RENDERING_FRAME_LATENCY)
            {
                unifex::sync_wait(prev_frame_rendering_jobs.front());
                prev_frame_rendering_jobs.pop();
            }
        }


        unifex::async_scope async_scope;

        async_scope.spawn_on(workers_.get_scheduler(), renderer_->renderFrame(delta_seconds));

        if constexpr (RENDERING_FRAME_LATENCY > 0) // NOLINT
        {
            prev_frame_rendering_jobs.push(async_scope.cleanup());
        }
        else
        {
            unifex::sync_wait(async_scope.cleanup());
        }
    }


    while (!prev_frame_rendering_jobs.empty())
    {
        unifex::sync_wait(prev_frame_rendering_jobs.front());
        prev_frame_rendering_jobs.pop();
    }

    run_all(query_for_tag<TGameLoopFinished>(world_));

    spdlog::info("Game loop finished successfully");

    return 0;
}
