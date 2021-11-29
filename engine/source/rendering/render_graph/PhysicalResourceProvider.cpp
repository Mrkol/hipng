#include "rendering/render_graph/PhysicalResourceProvider.hpp"

#include "rendering/render_graph/RenderGraph.hpp"


RG::PhysicalResourceProvider::PhysicalResourceProvider(RenderGraph& rendergraph, ExternalResourceInfo info)
    : render_graph_{rendergraph}
    , info_{std::move(info)}
{

}

