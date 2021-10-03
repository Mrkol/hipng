cmake_minimum_required(VERSION 3.20)

project(hipng)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

if(NOT DEFINED CMAKE_SUPPRESS_DEVELOPER_WARNINGS)
     set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE INTERNAL "No dev warnings")
endif()

# Get the magical CPM tool thingy
set(CPM_DOWNLOAD_VERSION 0.34.0)

if(CPM_SOURCE_CACHE)
  set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
  set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
  message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
  file(DOWNLOAD
       https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
       ${CPM_DOWNLOAD_LOCATION}
  )
endif()

include(${CPM_DOWNLOAD_LOCATION})

set(CPM_USE_LOCAL_PACKAGES True)



set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


find_program(glslang_validator glslangValidator)

function(target_add_shaders TARGET)
    list(POP_FRONT ${ARGN})

    set(shader_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders/")

    foreach(GLSL ${ARGN})
        get_filename_component(FILE_NAME ${GLSL} NAME)
        set(SPIRV "${shader_dir}/${FILE_NAME}.spv")
        add_custom_command(
                OUTPUT ${SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_dir}
                COMMAND ${glslang_validator} -V ${GLSL} -o ${SPIRV}
                DEPENDS ${GLSL})
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach(GLSL)

    set(custom_target_name "${TARGET}_shaders")
    add_custom_target(${custom_target_name} DEPENDS ${SPIRV_BINARY_FILES})
    add_dependencies(${TARGET} ${custom_target_name})
endfunction()


include(thirdparty.cmake)

add_subdirectory(engine)
add_subdirectory(editor)
