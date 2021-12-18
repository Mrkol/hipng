cmake_minimum_required(VERSION 3.20)

# TODO: public/interface/private
define_property(TARGET PROPERTY SHADER_INCLUDE_DIRECTORIES
    BRIEF_DOCS "Adds this include directories to all shaders of this target"
    FULL_DOCS "Adds this include directories to all shaders of this target")


find_program(glslang_validator glslangValidator)

function(list_transform_prepend var prefix)
    set(temp "")
    foreach(f ${${var}})
        list(APPEND temp "${prefix}${f}")
    endforeach()
    set(${var} "${temp}" PARENT_SCOPE)
endfunction()


function(target_add_shaders TARGET)
    list(POP_FRONT ${ARGN})

    set(shader_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
    get_property(incl_dirs TARGET ${TARGET} PROPERTY SHADER_INCLUDE_DIRECTORIES)
    list_transform_prepend(incl_dirs "-I")
    string(REPLACE ";" " " compl_flags "${incl_dirs}")

    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(compl_flags "${compl_flags}" -g)
    endif()

    set(compl_flags "${compl_flags}" -V)

    foreach(GLSL ${ARGN})
        get_filename_component(FILE_NAME ${GLSL} NAME)
        set(SPIRV "${shader_dir}/${FILE_NAME}.spv")
        add_custom_command(
                OUTPUT ${SPIRV}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_dir}
                COMMAND ${glslang_validator} ${compl_flags} ${GLSL} -o ${SPIRV}
                VERBATIM
                DEPENDS ${GLSL})
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach(GLSL)

    set(custom_target_name "${TARGET}_shaders")
    add_custom_target(${custom_target_name} DEPENDS ${SPIRV_BINARY_FILES})
    add_dependencies(${TARGET} ${custom_target_name})
endfunction()
