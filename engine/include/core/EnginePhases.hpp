#pragma once


#include <flecs.h>


// Systems marked with these tags will get run when the titular events happen


struct TGameLoopStarting {};
struct TGameLoopFinished {};

template<class Tag>
flecs::query<> query_for_tag(flecs::world& world)
{
    return world.query_builder()
        .term(flecs::System)
        .term<Tag>()
        .build();
}

inline void run_all(const flecs::query<>& q)
{
    q.each([](flecs::entity e)
    {
        flecs::system<>(e.world(), e).run();
    });
}
