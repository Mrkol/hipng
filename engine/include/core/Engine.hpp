#pragma once

#include <unifex/task.hpp>
#include <unifex/async_scope.hpp>
#include <flecs.h>

#include "rendering/RenderingSubsystem.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/BlockingThreadPool.hpp"
#include "core/EngineHandle.hpp"
#include "assets/AssetSubsystem.hpp"


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
    friend class EngineHandle;

public:
    Engine(int argc, char** argv);

    [[nodiscard]] flecs::world& world() { return world_; }
    [[nodiscard]] const flecs::world& world() const { return world_; }

    int run();
    
private:
    unifex::task<int> mainEventLoop();

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point last_tick_;

    flecs::world world_;
    
    ThreadPool main_thread_pool_;
    BlockingThreadPool blocking_thread_pool_;

    std::unique_ptr<RenderingSubsystem> renderer_;
    std::unique_ptr<AssetSubsystem> asset_subsystem_;

    std::size_t current_frame_idx_{0};
    std::size_t inflight_frames_ {2}; // TODO: replace with a config

    unifex::async_scope* current_frame_scope_{nullptr}; // TODO: should probably become atomic further down the line.
    ThreadPool::Scheduler::Sender os_polling_sender_{};
};
