cmake_minimum_required(VERSION 3.20)

CPMAddPackage("gh:SanderMertens/flecs#master")

CPMAddPackage("gh:facebookexperimental/libunifex#main")

CPMAddPackage(
    GITHUB_REPOSITORY Naios/function2
    GIT_TAG 4.2.0
)

CPMAddPackage(
    GITHUB_REPOSITORY jarro2783/cxxopts
    VERSION 2.2.1
    OPTIONS "CXXOPTS_BUILD_EXAMPLES NO" "CXXOPTS_BUILD_TESTS NO" "CXXOPTS_ENABLE_INSTALL YES"
)

find_package(Vulkan REQUIRED)

CPMAddPackage(
    NAME glfw3
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.3.2
    OPTIONS
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BULID_DOCS OFF"
)

CPMAddPackage(
    NAME VulkanMemoryAllocator
    GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

if (VulkanMemoryAllocator_ADDED)
    add_library(VulkanMemoryAllocator INTERFACE)
    target_include_directories(VulkanMemoryAllocator INTERFACE ${VulkanMemoryAllocator_SOURCE_DIR}/include/)
endif ()

CPMAddPackage(
    NAME ImGui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

if (ImGui_ADDED)
    add_library(DearImGui
        ${ImGui_SOURCE_DIR}/imgui.cpp ${ImGui_SOURCE_DIR}/imgui_draw.cpp
        ${ImGui_SOURCE_DIR}/imgui_tables.cpp ${ImGui_SOURCE_DIR}/imgui_widgets.cpp
        ${ImGui_SOURCE_DIR}/imgui_demo.cpp # TODO: remove
        ${ImGui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp ${ImGui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)

    target_include_directories(DearImGui PUBLIC ${ImGui_SOURCE_DIR})

    target_add_shaders(DearImGui
        ${ImGui_SOURCE_DIR}/backends/vulkan/glsl_shader.frag
        ${ImGui_SOURCE_DIR}/backends/vulkan/glsl_shader.vert)

    target_link_libraries(DearImGui Vulkan::Vulkan)
    target_link_libraries(DearImGui glfw)
endif ()

# Adds nlohmann's json and stbimage for free. Not sure whether I like this.
CPMAddPackage(
    NAME tinygltf
    GITHUB_REPOSITORY syoyo/tinygltf
    GIT_TAG master
    OPTIONS
        "TINYGLTF_HEADER_ONLY OFF"
        "TINYGLTF_INSTALL OFF"
)

CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.9.2
)

CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG master
)

CPMAddPackage(
    NAME yaml-cpp
    GITHUB_REPOSITORY jbeder/yaml-cpp
    GIT_TAG yaml-cpp-0.7.0
)
