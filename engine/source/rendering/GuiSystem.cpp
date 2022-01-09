#include "rendering/GuiSystem.hpp"

#include "core/WindowSystem.hpp"
#include "rendering/FramePacket.hpp"


ImDrawData* cloneDrawData(ImDrawData* list)
{
    auto result = IM_NEW(ImDrawData(*list));

    result->CmdLists = new ImDrawList*[result->CmdListsCount]; 
    for (int i = 0; i < result->CmdListsCount; ++i)
    {
        result->CmdLists[i] = list->CmdLists[i]->CloneOutput();
    }

    return result;
}

void deleteDrawData(ImDrawData* list)
{
	if (list == nullptr)
    {
        return;
    }

    for (int i = 0; i < list->CmdListsCount; ++i)
    {
        IM_FREE(list->CmdLists[i]);
    }

    delete[] list->CmdLists;
    list->CmdListsCount = 0;
    IM_FREE(list);
}


void register_gui_systems(flecs::world& world)
{
    world.observer<CWindow, const TRequiresGui>("Init imgui")
        .event(flecs::OnAdd)
		.each([](flecs::entity e, CWindow& window, const TRequiresGui&)
		{
		    // TODO: shared atlases
            auto context = GuiContextHolder{ImGui::CreateContext(), &ImGui::DestroyContext};
		    ImGui::SetCurrentContext(context.get());
		    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		    ImGui::StyleColorsDark();

		    ImGui_ImplGlfw_InitForVulkan(window.glfw_window.get(), true);

            e.remove<TRequiresGui>();
            e.set(CUninitializedGui{.context = std::move(context)});
		});

    world.system<CGui>("Start gui frame")
		.kind(flecs::PreUpdate)
		.each([](CGui& gui)
		{
			ImGui::SetCurrentContext(gui.context.get());
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
		});
    
    world.system<CGui>("Upload GUI data")
		.kind(flecs::PostUpdate)
		.each([](flecs::entity e, CGui& gui)
		{
			ImGui::SetCurrentContext(gui.context.get());
            ImGui::EndFrame();

            // dirty haxxx
            ImGui::Render();
            std::unique_ptr<ImDrawData, void(*)(ImDrawData*)> draw_data_copy(
                cloneDrawData(ImGui::GetDrawData()),
                &deleteDrawData);

			FramePacket* packet =
				e.world().component<CCurrentFramePacket>().get<CCurrentFramePacket>()->packet;

            packet->gui_packets.emplace(gui.context.get(), GuiFramePacket{
                std::move(draw_data_copy)
            });
		});
}
