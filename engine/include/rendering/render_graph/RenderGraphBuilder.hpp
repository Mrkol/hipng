#pragma once

#include "RenderGraph.hpp"
#include "Node.hpp"
#include "Resource.hpp"


namespace RG
{

class RenderGraphBuilder
{
public:
    Image& addImage(std::string_view name, Resource::Lifetime lifetime);
    RenderPass& addPass(std::string_view node);

    /**
     * Builds the graph
     * 0. Validates the graph
     * 1. Optimizes the graph
     * 2. Allocates resources
     * 3. Generates synchronization primitives
     *
     * The graph is stored in RenderGraphNode instances. The intention is that the result of optimization
     * MUST be manually representable by the render graph, therefore we optimize on the level of virtual
     * resources (i.e. RG::Resource and RG::Node), not physical ones
     */
    std::unique_ptr<RenderGraph> build();

private:
    void validate();
    void optimize();
    void allocate();
    void synchronize();


private:
    std::vector<std::unique_ptr<Resource>> resources_;
    std::vector<std::unique_ptr<Node>> nodes_;
};


}

