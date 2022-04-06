#include "core/WindowSystem.hpp"

#include <string>


flecs::entity create_window(flecs::world& world, WindowCreateInfo info)
{
    // String views are not necessarily C strings, so a copy is necessary
    std::string name_copy{info.name};

    auto entity =  world.entity(name_copy.c_str());

    if (entity.has<CWindow>())
    {
	    return entity;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto window = glfwCreateWindow(800, 600, name_copy.c_str(), nullptr, nullptr);

    entity.set(CWindow{
    		.glfw_window = {window, &glfwDestroyWindow},
	    });
    
    if (info.requiresImgui)
    {
        // immediately initializes
	    entity.add<TRequiresGui>();
    }

    if (info.requiresVulkan)
    {
	    entity.add<TRequiresVulkan>();
    }

    return entity;
}

void register_window_systems(flecs::world& world)
{
    world.system<CWindow>("Quit if main window closes")
        .term<TMainWindow>()
        .kind(flecs::PostUpdate)
        .iter([](flecs::iter it, CWindow* w)
        {
            for (auto i : it)
            {
                if (glfwWindowShouldClose(w[i].glfw_window.get()))
                {
                    it.world().quit();
                }
            }
        });
}
