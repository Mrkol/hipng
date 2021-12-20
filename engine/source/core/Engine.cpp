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
#include "core/GameplaySystem.hpp"
#include "core/WindowSystem.hpp"
#include "rendering/ActorSystem.hpp"
#include "rendering/VulkanSystem.hpp"
#include "rendering/FramePacket.hpp"


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
    register_actor_systems(world_);
    input_handler_ = InputHandler::register_input_systems(world_);

    asset_subsystem_ = std::make_unique<AssetSubsystem>(AssetSubsystem::CreateInfo{
        .base_path = NG_PROJECT_BASEPATH,
    });
}

int Engine::run()
{
    return unifex::sync_wait(mainEventLoop()).value_or(-1);
}

unifex::task<int> Engine::mainEventLoop()
{
    co_await unifex::schedule(g_engine.mainScheduler());

    
    spdlog::info("Game loop starting");

    // We have to poll GLFW on the same thread the window got created :/
    run_all(query_for_tag<TGameLoopStarting>(world_));

    // Kostyl, but it works.
    os_polling_sender_ = unifex::schedule_with_subscheduler(g_engine.mainScheduler());

    auto render_windows = world_.query<CWindow>();

    bool should_quit = false;

    StaticScope<EngineHandle::MAX_INFLIGHT_FRAMES, unifex::task<void>>
		rendering_scope(g_engine.inflightFrames());

    AssetHandle avocado{"engine/resources/avocado/Avocado.gltf"};
    auto model = co_await asset_subsystem_->loadModel(avocado);
    unifex::async_scope global_scope;
    // LOADING STUFF FROM A DIFFERENT THREAD! POG!
	global_scope.spawn(renderer_->getGpuStorageManager().uploadStaticMesh(avocado, model));

    world_.entity("AVOCADINA")
		.set<CPosition>(CPosition{
			.position = {0, 0, 0},
			.rotation = glm::quat({0, glm::pi<float>()/4, glm::pi<float>()/4}),
		})
		.set<CStaticMeshActor>(CStaticMeshActor{
			.model = avocado,
            .scale = 1000,
        }).is_a(world_.entity("ListensToInputEvents"));
    
    world_.entity("AVOCADINA2")
		.set<CPosition>(CPosition{
			.position = {0, 0, 0},
			.rotation = glm::quat({0, glm::pi<float>()/4, -glm::pi<float>()/4}),
		})
		.set<CStaticMeshActor>(CStaticMeshActor{
			.model = avocado,
            .scale = 1000,
		}).is_a(world_.entity("ListensToInputEvents"));

    world_.entity("camera")
		.set<CPosition>(CPosition{
			.position = {0, -0.1, 0},
			.rotation = quatLookAt(glm::vec3(1, 0, 0), glm::vec3(0, 0, 1))
		})
		.set<CCameraActor>(CCameraActor{ .fovx = 100, .fovy = 90, .near = 0.01f, .far = 100.f })
		.add<TActiveCamera>();

    while (!should_quit)
    {
        co_await os_polling_sender_;

        unifex::async_scope frame_scope;
        current_frame_scope_ = &frame_scope;

        ++current_frame_idx_;

        glfwPollEvents();

        input_handler_->Update();

        auto this_tick = Clock::now();
        float delta_seconds =
            std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1>>>(this_tick - last_tick_).count();
        last_tick_ = this_tick;

        FramePacket packet;

        world_.component<CCurrentFramePacket>()
			.set(CCurrentFramePacket{&packet});

        should_quit |= !world_.progress(delta_seconds);

        world_.component<CCurrentFramePacket>()
			.set(CCurrentFramePacket{nullptr});

        co_await rendering_scope.spawn_next(renderer_->renderFrame(current_frame_idx_, std::move(packet)));

        co_await unifex::on(g_engine.mainScheduler(), frame_scope.cleanup());
        current_frame_scope_ = nullptr;
    }

    co_await rendering_scope.all_finished();
    co_await unifex::on(g_engine.mainScheduler(), global_scope.cleanup());

    run_all(query_for_tag<TGameLoopFinished>(world_));
    
    main_thread_pool_.request_stop();
    blocking_thread_pool_.request_stop();

    spdlog::info("Game loop finished successfully");

    co_return 0;
}
