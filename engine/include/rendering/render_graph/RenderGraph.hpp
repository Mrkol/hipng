#pragma once

#include <unordered_map>
#include <function2/function2.hpp>
#include <vulkan/vulkan.hpp>

#include "rendering/render_graph/PhysicalResourceProvider.hpp"
#include "rendering/render_graph/Node.hpp"


namespace RG
{

class RenderGraph
{
    RenderGraph() = default;
public:
    friend class PhysicalResourceProvider;
    friend class RenderGraphBuilder;

    void compile(vk::CommandBuffer& cb, ExternalResourceInfo info);
private:
    static constexpr std::size_t NONE = std::numeric_limits<std::size_t>::max();

    // TODO: framebuffer sharing
    std::vector<vk::UniqueFramebuffer> framebuffers_;

    struct PhysicalRenderPass
    {
        vk::Framebuffer framebufer;
        vk::UniqueRenderPass renderpass;
    };

    std::vector<PhysicalRenderPass> renderpasses_;
    std::unordered_map<std::string, std::size_t> renderpass_indices_;

    struct PhysicalEvent
    {
        vk::UniqueEvent event;
        vk::DependencyInfoKHR dependency;
    };

    // TODO: Event aliasing????
    std::vector<PhysicalEvent> events_;

    std::vector<std::unique_ptr<Node>> node_sequence_;
};

}
