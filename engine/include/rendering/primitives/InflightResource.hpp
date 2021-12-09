#pragma once

#include <array>

#include "core/EngineHandle.hpp"
#include "util/Mixins.hpp"


template<class T>
class InflightResource : public NoMove
{
public:
	template<class... Args>
	explicit InflightResource(std::in_place_t, const Args&... args)
	{
		for (std::size_t i = 0; i < g_engine.inflightFrames(); ++i)
		{
			impl_[i].construct(args...);
		}
	}

	explicit InflightResource(const std::invocable<std::size_t> auto& f)
	{
		for (std::size_t i = 0; i < g_engine.inflightFrames(); ++i)
		{
			impl_[i].construct_with(
				[&f, i]()
				{
					return f(i);
				});
		}
	}
	
	T* get(std::size_t idx)
		{ return std::addressof(impl_[idx % g_engine.inflightFrames()].get()); }
	const T* get(std::size_t idx) const
		{ return std::addressof(impl_[idx % g_engine.inflightFrames()]); }
	
	T* get_previous(std::size_t idx)
		{ return std::addressof(impl_[(idx + g_engine.inflightFrames() - 1) % g_engine.inflightFrames()].get()); }
	const T* get_previous(std::size_t idx) const
	{ return std::addressof(impl_[(idx + g_engine.inflightFrames() - 1) % g_engine.inflightFrames()].get()); }


	~InflightResource() noexcept
	{
		for (std::size_t i = 0; i < g_engine.inflightFrames(); ++i)
		{
			impl_[i].destruct();
		}
	}
	
private:
	std::array<unifex::manual_lifetime<T>, EngineHandle::MAX_INFLIGHT_FRAMES> impl_;
};
