#pragma once


class IRenderer
{
public:
    struct RenderingDone
    {
        vk::Semaphore sem;
        vk::Fence fence;
    };

    /**
     * Rationale for passing in the present image view is that when it changes, some framebuffers need
     * recreation. Keeping track of this should be the renderer's responsibility, the view is the only
     * thing bridging the windowing and rendering systems.
     */
    virtual RenderingDone render(std::size_t frame_index, vk::ImageView present_image, vk::Semaphore image_available) = 0;

    /**
     *
     * @param target -- Views to swapchain images
     * @param resolution -- New resolution
     */
    virtual void updatePresentationTarget(std::span<vk::ImageView> target, vk::Extent2D resolution) = 0;

    virtual ~IRenderer() = default;
};
