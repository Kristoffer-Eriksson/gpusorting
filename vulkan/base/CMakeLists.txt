cmake_minimum_required(VERSION 3.24)

project(VulkanGettingStarted)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

function(compile_shader target source stage)
    get_filename_component(name ${source} NAME_WLE)
    set(spv_path ${CMAKE_CURRENT_BINARY_DIR}/${name}.spv)
    add_custom_command(
        OUTPUT ${spv_path}
        DEPENDS ${source}
        DEPFILE ${CMAKE_CURRENT_BINARY_DIR}/${name}.d
        COMMAND
            ${glslc_executable}
            --target-env=vulkan1.3
            -fshader-stage=${stage}
            -MD -MF ${CMAKE_CURRENT_BINARY_DIR}/${name}.d
            -o ${spv_path}
            ${source}
    )
    target_sources(${target} PRIVATE ${spv_path})
    install(FILES ${spv_path} DESTINATION lib)
    target_compile_definitions(${target}
        PRIVATE
            VULKAN_GS_DEFAULT_${stage}_SHADER_PATH="${CMAKE_INSTALL_PREFIX}/lib/${name}.spv"
    )
    set(VULKAN_GS_DEFAULT_${stage}_SHADER_PATH ${CMAKE_INSTALL_PREFIX}/lib/${name}.spv PARENT_SCOPE)
endfunction()

# Declare target
add_executable(vulkangs src/main.cpp)

# Compile shaders
compile_shader(vulkangs ${CMAKE_CURRENT_SOURCE_DIR}/src/default.glsl compute)

# Target config
target_link_libraries(vulkangs Vulkan::Vulkan)

# Autogenerate .vscode settings
string(CONCAT workspaceFolder "$" "{workspaceFolder}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.vscode/c_cpp_properties.json.in ${CMAKE_CURRENT_SOURCE_DIR}/.vscode/c_cpp_properties.json)

# Install
install(TARGETS
    vulkangs
    EXPORT VulkanGettingStartedTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
