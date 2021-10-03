#include "core/WindowSystem.hpp"


CWindow create_window(std::string_view name)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // String views are not necessarily C strings, so a copy is necessary
    std::string copy{name};
    return CWindow{{glfwCreateWindow(800, 600, copy.c_str(), nullptr, nullptr), &glfwDestroyWindow}};
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
