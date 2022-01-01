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

EventQueue::Scheduler EngineHandle::nextFrameScheduler()
{
    return engine_->next_frame_events_.get_scheduler();
}

void EngineHandle::async(unifex::any_sender_of<> task)
{
    engine_->global_scope_.spawn(std::move(task));
}

flecs::world& EngineHandle::world()
{
    return engine_->world_;
}

std::size_t EngineHandle::inflightFrames() const
{
    return engine_->inflight_frames_;
}
