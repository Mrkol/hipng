#include "core/EngineHandle.hpp"

#include "core/Engine.hpp"


ThreadPool::Scheduler::Sender EngineHandle::scheduleOsPolling()
{
    return engine_->os_polling_sender_;
}

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
    NG_ASSERTF(engine_->current_frame_scope_ != nullptr, "asyncThisFrame can only be called from a frame context!");
    engine_->current_frame_scope_->spawn(std::move(task));
}

flecs::world& EngineHandle::world()
{
    return engine_->world_;
}

std::size_t EngineHandle::inflightFrames() const
{
    return engine_->inflight_frames_;
}
