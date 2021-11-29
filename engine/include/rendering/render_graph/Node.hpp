#pragma once

#include <vulkan/vulkan.hpp>
#include <function2/function2.hpp>

#include "rendering/render_graph/Resource.hpp"


namespace RG
{

class PhysicalResourceProvider;


class Node
{
public:
    enum class Type
    {
        RenderPass,
        // ComputePass,
        // ImageBlit,
        // etc
    };

    explicit Node(Type type);

    virtual void compile(vk::CommandBuffer& cb, PhysicalResourceProvider& rp) = 0;

    virtual ~Node() = default;
private:
    Type type_;
};

/*
 * Represents a renderpass of arbitrary complexity.
 * Should be created via a builder. Can be optimized/combined in the context of render graph.
 */
class RenderPass final : public Node
{
public:
    friend class RenderGraphBuilder;

    RenderPass();

    void compile(vk::CommandBuffer& cb, PhysicalResourceProvider& rp) override;

private:
    // here be dragons.
    // needs to support every single thing that actual VK renderpasses support
    fu2::unique_function<void(vk::CommandBuffer&, PhysicalResourceProvider&)> compile_;

    std::vector<vk::ClearValue> clear_values_;
    vk::Rect2D render_area_;

    std::vector<std::string> color_attachments_;
};

}

