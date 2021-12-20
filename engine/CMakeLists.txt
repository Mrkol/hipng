cmake_minimum_required(VERSION 3.20)



file(GLOB_RECURSE hipengine_source ${CMAKE_CURRENT_LIST_DIR}/source/*.cpp)
file(GLOB_RECURSE hipengine_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders/*)

# Exclude shader "includes"
list(FILTER hipengine_shaders EXCLUDE REGEX ".*\\.h$")


add_library(hipengine ${hipengine_source})
target_add_shaders(hipengine ${hipengine_shaders})

target_compile_definitions(
	hipengine
	PUBLIC
	VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
	VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
	TINYGLTF_USE_CPP14
	NG_PROJECT_BASEPATH="${PROJECT_SOURCE_DIR}"
	)

target_include_directories(hipengine
	PUBLIC
	"include"
	# TODO: make public includes not depend on the shader bridge
	"shaders" # Shaders are included only for the shader_cpp_bridge files
	PRIVATE
	"source"
	)

# We link vulkan pfns dynamically
target_include_directories(hipengine PUBLIC $<TARGET_PROPERTY:Vulkan::Vulkan,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(hipengine
	PUBLIC
	Threads::Threads
	function2
	spdlog
	unifex
	flecs::flecs_static
	VulkanMemoryAllocator
	DearImGui
	glm 
	tinygltf

	PRIVATE
	cxxopts
	glfw
	)
