#include "rendering/render_graph/RenderGraphBuilder.hpp"


RG::Image& RG::RenderGraphBuilder::addImage(std::string_view name, Resource::Lifetime lifetime)
{
    auto* ptr = static_cast<Image*>(resources_.emplace_back(new Image).get());
    ptr->name_ = name;
    ptr->type_ = Resource::Type::Image;
    ptr->lifetime_ = lifetime;
    return *ptr;
}

RG::RenderPass &RG::RenderGraphBuilder::addPass(std::string_view node)
{
    return *static_cast<RenderPass*>(nodes_.emplace_back(new RenderPass).get());
}

std::unique_ptr<RG::RenderGraph> RG::RenderGraphBuilder::build()
{

}
