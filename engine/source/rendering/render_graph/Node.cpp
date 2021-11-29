#include "rendering/render_graph/Node.hpp"

#include "rendering/render_graph/PhysicalResourceProvider.hpp"


RG::Node::Node(Type type)
        : type_(type)
{
}

void RG::RenderPass::compile(vk::CommandBuffer& cb, PhysicalResourceProvider& rp)
{
    cb.beginRenderPass2(
        vk::RenderPassBeginInfo{
            .renderPass = rp.get_pass(*this),
            .framebuffer = rp.get_framebuffer(*this),
            .renderArea = render_area_,
            .clearValueCount = static_cast<uint32_t>(clear_values_.size()),
            .pClearValues = clear_values_.data(),
        },
        vk::SubpassBeginInfo{
            .contents = vk::SubpassContents::eInline
        });

    compile_(cb, rp);

    cb.endRenderPass2(vk::SubpassEndInfo{});
}

RG::RenderPass::RenderPass()
        : Node(Type::RenderPass)
{

}


