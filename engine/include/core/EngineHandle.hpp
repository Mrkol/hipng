#pragma once

#include <flecs.h>

#include "concurrency/ThreadPool.hpp"
#include "concurrency/BlockingThreadPool.hpp"


class Engine;

class EngineHandle
{
public:
    explicit EngineHandle(Engine* engine) : engine_{engine} {}

    ThreadPool::Scheduler main_scheduler();
    BlockingThreadPool::Scheduler blocking_scheduler();
    flecs::world& world();

private:
    Engine* engine_;
};

extern EngineHandle g_engine;
