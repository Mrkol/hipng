cmake_minimum_required(VERSION 3.20)



file(GLOB_RECURSE hipengine_source ${CMAKE_CURRENT_LIST_DIR}/source/*.cpp)
file(GLOB_RECURSE hipengine_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders/*)

add_definitions(-DVULKAN_HPP_NO_STRUCT_CONSTRUCTORS)
add_library(hipengine ${hipengine_source})
target_add_shaders(hipengine ${hipengine_shaders})
target_include_directories(hipengine PUBLIC "include")
target_include_directories(hipengine PRIVATE "source")
# Shaders are included only for the shader_cpp_bridge files
target_include_directories(hipengine PRIVATE "shaders")


target_link_libraries(hipengine PUBLIC Threads::Threads)
target_link_libraries(hipengine PUBLIC function2)
target_link_libraries(hipengine PUBLIC unifex)
target_link_libraries(hipengine PUBLIC flecs::flecs_static)
target_link_libraries(hipengine PUBLIC Vulkan::Vulkan)
target_link_libraries(hipengine PRIVATE cxxopts)
target_link_libraries(hipengine PRIVATE glfw)
target_link_libraries(hipengine PUBLIC VulkanMemoryAllocator)
target_link_libraries(hipengine PUBLIC DearImGui)
target_link_libraries(hipengine PRIVATE assimp::assimp)
target_link_libraries(hipengine PUBLIC spdlog)

