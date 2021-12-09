#pragma once

#include <concepts>


template<std::invocable<> T>
class Defer
{
public:
	Defer(T t) : t_{t} {}

	~Defer() 
	{
		t_();
	}

private:
	T t_;
};
