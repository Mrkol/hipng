#include "core/EngineHandle.hpp"
#include "core/Engine.hpp"


ThreadPool::Scheduler EngineHandle::main_scheduler()
{
	return engine_->main_thread_pool_.get_scheduler();
}

BlockingThreadPool::Scheduler EngineHandle::blocking_scheduler()
{
	return engine_->blocking_thread_pool_.get_scheduler();
}

flecs::world& EngineHandle::world()
{
	return engine_->world_;
}
