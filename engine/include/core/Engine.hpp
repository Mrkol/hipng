#pragma once

#include <unifex/static_thread_pool.hpp>
#include <flecs.h>

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

    int run();

private:
    int game_loop();

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point last_tick_;

    flecs::world world_;
    //unifex::static_thread_pool pool_;
    
    flecs::entity main_window_;
};


