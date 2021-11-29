#pragma once

namespace RG
{

class Resource
{
public:
    enum class Type
    {
        Invalid,
        Buffer,
        Image
    };

    enum class Lifetime
    {
        // Managed by rendergraph, only used for intermediate resource storage
        Transient,
        // Managed by special persistent resource system, need to persist between frames,
        // should be used for TAA and similar stuff (TODO)
        Persistent,
        // Managed externally
        External
    };



    virtual ~Resource() = default;

protected:
    std::string name_;
    Lifetime lifetime_;
    Type type_;
};

class Image : public Resource
{
public:
    friend class RenderGraphBuilder;

protected:
    int kek;
};

}
