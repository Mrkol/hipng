#pragma once

#include <flecs.h>
#include <unifex/task.hpp>

#include "concurrency/ThreadPool.hpp"
#include "concurrency/BlockingThreadPool.hpp"


class Engine;

/**
 * Class for lightweight access to engine features.
 * Should not include as much stuff as the engine does.
 * Kind of works like pimpl.
 */
class EngineHandle
{
public:
	/**
	 * This is just an upper bound, not the actual amount used.
	 * Use this to allocate memory statically.
	 */
    static constexpr std::size_t MAX_INFLIGHT_FRAMES = 4;


    explicit EngineHandle(Engine* engine) : engine_{engine} {}

    /**
     * Use this scheduler for most work.
     */
    ThreadPool::Scheduler mainScheduler();

    /**
     * Use this scheduler for long, blocking tasks
     */
    BlockingThreadPool::Scheduler blockingScheduler();


    void asyncThisFrame(unifex::task<void> task);


    flecs::world& world();
    

    /**
     * The actual amount of inflight frames.
     */
    std::size_t inflightFrames() const;

private:
    Engine* engine_;
};

extern EngineHandle g_engine;
