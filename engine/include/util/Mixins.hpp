#pragma once

struct NoCopy
{
    NoCopy() = default;

    NoCopy(const NoCopy&) = delete;
    NoCopy& operator=(const NoCopy&) = delete;

    NoCopy(NoCopy&&) = default;
    NoCopy& operator=(NoCopy&&) = default;
};

// I reject the notion of "copyable but non-movable" classes.
struct NoMove
{
    NoMove() = default;

    NoMove(const NoMove&) = delete;
    NoMove& operator=(const NoMove&) = delete;

    NoMove(NoMove&&) = delete;
    NoMove& operator=(NoMove&&) = delete;
};

