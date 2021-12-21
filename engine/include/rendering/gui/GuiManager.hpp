#pragma once

#include <vulkan/vulkan.hpp>
#include <imgui.h>


struct GuiFramePacket;

class GuiManager
{
public:
    struct CreateInfo
    {
        vk::Instance instance;
        vk::PhysicalDevice physical_device;
        vk::Device device;

        uint32_t graphics_queue_idx{};
        vk::Queue graphics_queue;
        std::size_t swapchain_size{};
        vk::Format format{};

        ImGuiContext* context;
    };

	explicit GuiManager(CreateInfo info);

    void render(vk::CommandBuffer cb, GuiFramePacket& packet);

    vk::RenderPass getRenderPass() { return render_pass_.get(); }

    ~GuiManager();

private:
    ImGuiContext* context_;

    vk::UniqueDescriptorPool descriptor_pool_;
    vk::UniqueRenderPass render_pass_;
};
