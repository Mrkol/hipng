#pragma once

#include <unordered_map>

#include <vulkan/vulkan.hpp>


namespace RG
{

class RenderGraph;
class RenderPass;

struct ExternalResourceInfo
{
    std::unordered_map<std::string, vk::ImageView> external_images;
    std::unordered_map<std::string, vk::Buffer> external_buffers;
};

class PhysicalResourceProvider {
public:
    PhysicalResourceProvider(RenderGraph& rendergraph, ExternalResourceInfo info);

    vk::ImageView get_image(std::string_view name);

    vk::Buffer get_buffer(std::string_view name);

    vk::RenderPass get_pass(const RenderPass& pass);
    vk::Framebuffer get_framebuffer(const RenderPass& pass);

private:
    RenderGraph& render_graph_;
    ExternalResourceInfo info_;
};


} // RG

