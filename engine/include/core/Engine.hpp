#pragma once

#include <unifex/thread_unsafe_event_loop.hpp>
#include <unifex/task.hpp>
#include <flecs.h>

#include "rendering/GlobalRenderer.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/BlockingThreadPool.hpp"
#include "core/EngineHandle.hpp"


constexpr auto APP_NAME = "HipNg";

// Forces GLFW to initialize before everything else via an inheritance trick
struct EngineBase
{
    EngineBase();
    ~EngineBase();

    EngineBase(const EngineBase&) = delete;
    EngineBase(EngineBase&&) = delete;
    EngineBase& operator=(const EngineBase&) = delete;
    EngineBase& operator=(EngineBase&&) = delete;
};

class Engine : EngineBase
{
public:
    static constexpr std::size_t MAX_INFLIGHT_FRAMES = 1;

    friend class EngineHandle;

public:
    Engine(int argc, char** argv);

    [[nodiscard]] flecs::world& world() { return world_; }
    [[nodiscard]] const flecs::world& world() const { return world_; }

    int run();
    
private:
    unifex::task<int> main_event_loop();

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point last_tick_;

    flecs::world world_;
    
    ThreadPool main_thread_pool_;
    BlockingThreadPool blocking_thread_pool_;

    std::unique_ptr<GlobalRenderer> renderer_;
};
