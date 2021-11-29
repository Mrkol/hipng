#include "rendering/render_graph/RenderGraph.hpp"


void RG::RenderGraph::compile(vk::CommandBuffer& cb, ExternalResourceInfo info)
{
    PhysicalResourceProvider rp(*this, std::move(info));

    for (auto& node : node_sequence_)
    {
        // submit wait for event
        node->compile(cb, rp);
        // submit signal event
    }
}
