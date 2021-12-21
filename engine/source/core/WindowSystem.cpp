#include "core/WindowSystem.hpp"

#include <span>

#include "rendering/FramePacket.hpp"


CWindow create_window(std::string_view name)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // String views are not necessarily C strings, so a copy is necessary
    std::string copy{name};
    auto window = glfwCreateWindow(800, 600, copy.c_str(), nullptr, nullptr);

    // TODO: shared atlases
    auto context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);
    return CWindow{
    	.glfw_window = {window, &glfwDestroyWindow},
    	.imgui_context = {context, &ImGui::DestroyContext}
    };
}

void register_window_systems(flecs::world& world)
{
    world.system<CWindow>()
		.kind(flecs::PostUpdate)
		.each([](flecs::entity e, CWindow&)
		{
			e.add<THasGui>();
		});

    world.system<CWindow>("Start gui frame")
		.kind(flecs::PreUpdate)
		.term<THasGui>()
		.each([](CWindow& window)
		{
			ImGui::SetCurrentContext(window.imgui_context.get());
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
		});
    
    world.system<CWindow>("Upload GUI data")
		.kind(flecs::PostUpdate)
		.term<THasGui>()
		.each([](flecs::entity e, CWindow& window)
		{
			ImGui::SetCurrentContext(window.imgui_context.get());
            ImGui::EndFrame();


            
            // dirty haxxx
            ImGui::Render();
            std::unique_ptr<ImDrawData, void(*)(ImDrawData*)> draw_data_copy(
                new ImDrawData(*ImGui::GetDrawData()),
                [](ImDrawData* data)
                {
	                for (int i = 0; i < data->CmdListsCount; ++i)
	                {
		                IM_FREE(data->CmdLists[i]);
                        delete[] data->CmdLists;
	                }
                    delete data;
                });

            auto old_lists = draw_data_copy->CmdLists;
            draw_data_copy->CmdLists = new ImDrawList*[draw_data_copy->CmdListsCount]; 
            for (int i = 0; i < draw_data_copy->CmdListsCount; ++i)
            {
	            draw_data_copy->CmdLists[i] = old_lists[i]->CloneOutput();
            }

            
			FramePacket* packet =
				e.world().component<CCurrentFramePacket>().get<CCurrentFramePacket>()->packet;

            packet->gui_packets.emplace(window.imgui_context.get(), GuiFramePacket{
                std::move(draw_data_copy)
            });
		});

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
