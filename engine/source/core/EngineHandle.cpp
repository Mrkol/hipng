#include "core/EngineHandle.hpp"

#include "core/Engine.hpp"


ThreadPool::Scheduler EngineHandle::mainScheduler()
{
    return engine_->main_thread_pool_.get_scheduler();
}

BlockingThreadPool::Scheduler EngineHandle::blockingScheduler()
{
    return engine_->blocking_thread_pool_.get_scheduler();
}

void EngineHandle::asyncThisFrame(unifex::task<void> task)
{
    NG_ASSERTF(engine_->current_flecs_scope_ != nullptr, "asyncThisFrame can only be called from a flecs system!");
    engine_->current_flecs_scope_->spawn(std::move(task));
}

flecs::world& EngineHandle::world()
{
    return engine_->world_;
}

std::size_t EngineHandle::inflightFrames() const
{
    return engine_->inflight_frames_;
}
