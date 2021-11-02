#pragma once

#include <unifex/static_thread_pool.hpp>
#include <unifex/thread_unsafe_event_loop.hpp>
#include <unifex/task.hpp>
#include <flecs.h>

#include "rendering/GlobalRenderer.hpp"


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
    Engine(int argc, char** argv);

    [[nodiscard]] flecs::world& world() { return world_; }
    [[nodiscard]] const flecs::world& world() const { return world_; }

    int run();

private:
    int main_event_loop();

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point last_tick_;

    flecs::world world_;

    unifex::static_thread_pool workers_;
    unifex::thread_unsafe_event_loop main_loop_events_;

    std::unique_ptr<GlobalRenderer> renderer_;
};
