#pragma once

#include <tuple>
#include <mutex>


template<class... Locks>
class MultiLock
{
public:
	explicit MultiLock(Locks&... locks) : locks_{locks...} {}

	void lock()
	{
		std::apply([](auto&... locks) { std::lock(locks...); }, locks_);
	}

	void unlock()
	{
		std::apply([](auto&... locks) { (locks.unlock(), ...); }, locks_);
	}

private:
	std::tuple<Locks&...> locks_;
};

