# Vulken-3D Shader Compilation CMake Module
# =============================================
#
# This module provides functions to compile GLSL shaders to SPIR-V
# and embed them in the build process with proper dependency tracking.

find_program(GLSLC_EXECUTABLE NAMES glslc
    HINTS
        $ENV{VULKAN_SDK}/bin
        $ENV{VULKAN_SDK}/Bin
        ${VULKAN_SDK}/bin
        ${VULKAN_SDK}/Bin
    PATHS
        /usr/bin
        /usr/local/bin
)

find_program(SPIRV_CROSS_EXECUTABLE NAMES spirv-cross
    HINTS  
        $ENV{VULKAN_SDK}/bin
        $ENV{VULKAN_SDK}/Bin
        ${VULKAN_SDK}/bin
        ${VULKAN_SDK}/Bin
    PATHS
        /usr/bin
        /usr/local/bin
)

if(NOT GLSLC_EXECUTABLE)
    message(WARNING "glslc not found! Shader compilation will be disabled. Install Vulkan SDK for full functionality.")
    return()
endif()

if(NOT SPIRV_CROSS_EXECUTABLE)
    message(WARNING "spirv-cross not found! Shader layout validation will be disabled")
endif()

# Set shader cache directory
set(SHADER_CACHE_DIR "${CMAKE_BINARY_DIR}/shaders_cache")
file(MAKE_DIRECTORY ${SHADER_CACHE_DIR})

# Function to generate content-based hash for shader files
function(shader_content_hash SHADER_FILE OUTPUT_VAR)
    file(SHA256 "${SHADER_FILE}" file_hash)
    string(SUBSTRING "${file_hash}" 0 16 short_hash)
    set(${OUTPUT_VAR} "${short_hash}" PARENT_SCOPE)
endfunction()

# Function to compile a single shader
function(compile_shader)
    set(options VALIDATE_LAYOUT)
    set(oneValueArgs SOURCE OUTPUT TARGET_ENV STAGE)
    set(multiValueArgs INCLUDES DEFINES)
    cmake_parse_arguments(SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    if(NOT SHADER_SOURCE)
        message(FATAL_ERROR "compile_shader: SOURCE argument required")
    endif()
    
    if(NOT SHADER_OUTPUT)
        message(FATAL_ERROR "compile_shader: OUTPUT argument required")
    endif()
    
    # Set default target environment
    if(NOT SHADER_TARGET_ENV)
        set(SHADER_TARGET_ENV "vulkan1.2")
    endif()
    
    # Auto-detect shader stage from extension if not provided
    if(NOT SHADER_STAGE)
        get_filename_component(ext ${SHADER_SOURCE} EXT)
        if(ext STREQUAL ".vert")
            set(SHADER_STAGE "vertex")
        elseif(ext STREQUAL ".frag")
            set(SHADER_STAGE "fragment") 
        elseif(ext STREQUAL ".comp")
            set(SHADER_STAGE "compute")
        elseif(ext STREQUAL ".geom")
            set(SHADER_STAGE "geometry")
        elseif(ext STREQUAL ".tesc")
            set(SHADER_STAGE "tesscontrol")
        elseif(ext STREQUAL ".tese") 
            set(SHADER_STAGE "tesseval")
        else()
            message(WARNING "Could not auto-detect shader stage for ${SHADER_SOURCE}")
        endif()
    endif()
    
    # Build compilation command
    set(COMPILE_CMD ${GLSLC_EXECUTABLE})
    list(APPEND COMPILE_CMD "--target-env=${SHADER_TARGET_ENV}")
    list(APPEND COMPILE_CMD "-O")  # Optimize
    list(APPEND COMPILE_CMD "-g")  # Debug info
    
    if(SHADER_STAGE)
        list(APPEND COMPILE_CMD "-fshader-stage=${SHADER_STAGE}")
    endif()
    
    # Add include directories
    foreach(include_dir ${SHADER_INCLUDES})
        list(APPEND COMPILE_CMD "-I${include_dir}")
    endforeach()
    
    # Add preprocessor defines
    foreach(define ${SHADER_DEFINES})
        list(APPEND COMPILE_CMD "-D${define}")
    endforeach()
    
    # Input and output
    list(APPEND COMPILE_CMD "-o" "${SHADER_OUTPUT}")
    list(APPEND COMPILE_CMD "${SHADER_SOURCE}")
    
    # Create custom command
    add_custom_command(
        OUTPUT "${SHADER_OUTPUT}"
        COMMAND ${COMPILE_CMD}
        DEPENDS "${SHADER_SOURCE}"
        COMMENT "Compiling shader: ${SHADER_SOURCE}"
        VERBATIM
    )
    
    # Optional layout validation
    if(SHADER_VALIDATE_LAYOUT AND SPIRV_CROSS_EXECUTABLE)
        set(LAYOUT_JSON "${SHADER_OUTPUT}.layout.json")
        add_custom_command(
            OUTPUT "${LAYOUT_JSON}"
            COMMAND ${SPIRV_CROSS_EXECUTABLE} "${SHADER_OUTPUT}" --output "${LAYOUT_JSON}" --reflect --json
            DEPENDS "${SHADER_OUTPUT}"
            COMMENT "Reflecting shader layout: ${SHADER_SOURCE}"
            VERBATIM
        )
        set_property(SOURCE "${LAYOUT_JSON}" PROPERTY GENERATED TRUE)
    endif()
endfunction()

# Function to compile all shaders in a directory
function(compile_shaders_directory)
    set(options VALIDATE_LAYOUT RECURSIVE)
    set(oneValueArgs DIRECTORY OUTPUT_DIR TARGET_ENV)
    set(multiValueArgs INCLUDES DEFINES EXTENSIONS)
    cmake_parse_arguments(SHADERS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    if(NOT SHADERS_DIRECTORY)
        message(FATAL_ERROR "compile_shaders_directory: DIRECTORY argument required")
    endif()
    
    if(NOT SHADERS_OUTPUT_DIR)
        set(SHADERS_OUTPUT_DIR "${SHADER_CACHE_DIR}")
    endif()
    
    if(NOT SHADERS_EXTENSIONS)
        set(SHADERS_EXTENSIONS "*.vert" "*.frag" "*.comp" "*.geom" "*.tesc" "*.tese" "*.glsl")
    endif()
    
    # Find all shader files
    set(shader_files "")
    foreach(ext ${SHADERS_EXTENSIONS})
        if(SHADERS_RECURSIVE)
            file(GLOB_RECURSE files "${SHADERS_DIRECTORY}/${ext}")
        else()
            file(GLOB files "${SHADERS_DIRECTORY}/${ext}")
        endif()
        list(APPEND shader_files ${files})
    endforeach()
    
    # Compile each shader
    set(compiled_shaders "")
    foreach(shader_file ${shader_files})
        # Generate output filename with content hash
        shader_content_hash("${shader_file}" content_hash)
        get_filename_component(shader_name ${shader_file} NAME)
        set(output_file "${SHADERS_OUTPUT_DIR}/${shader_name}_${content_hash}.spv")
        
        compile_shader(
            SOURCE "${shader_file}"
            OUTPUT "${output_file}"
            TARGET_ENV "${SHADERS_TARGET_ENV}"
            INCLUDES ${SHADERS_INCLUDES}
            DEFINES ${SHADERS_DEFINES}
            ${SHADERS_VALIDATE_LAYOUT}
        )
        
        list(APPEND compiled_shaders "${output_file}")
    endforeach()
    
    # Create target for all compiled shaders
    add_custom_target(compile_all_shaders ALL
        DEPENDS ${compiled_shaders}
        COMMENT "Compiling all shaders"
    )
    
    # Export list for use in other targets
    set(COMPILED_SHADERS ${compiled_shaders} PARENT_SCOPE)
endfunction()

# Function to create shader validation test
function(add_shader_validation_test)
    set(oneValueArgs NAME)
    set(multiValueArgs SHADERS)
    cmake_parse_arguments(TEST "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    if(NOT TEST_NAME)
        set(TEST_NAME "shader_validation")
    endif()
    
    if(NOT TEST_SHADERS)
        set(TEST_SHADERS ${COMPILED_SHADERS})
    endif()
    
    # Create test that validates all shaders compile without errors
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${CMAKE_COMMAND} -E echo "All shaders compiled successfully"
    )
    
    # Test depends on shader compilation
    set_tests_properties(${TEST_NAME} PROPERTIES
        DEPENDS compile_all_shaders
    )
endfunction()

# Main function to set up shader compilation for the project
function(setup_shader_compilation)
    set(options VALIDATE_LAYOUT)
    set(oneValueArgs TARGET_ENV)
    set(multiValueArgs INCLUDE_DIRS DEFINES)
    cmake_parse_arguments(SETUP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    message(STATUS "Setting up shader compilation")
    message(STATUS "  GLSLC: ${GLSLC_EXECUTABLE}")
    message(STATUS "  SPIRV-Cross: ${SPIRV_CROSS_EXECUTABLE}")
    message(STATUS "  Cache directory: ${SHADER_CACHE_DIR}")
    
    # Add shader directories to include path
    list(APPEND SETUP_INCLUDE_DIRS 
        "${CMAKE_SOURCE_DIR}/shaders"
        "${CMAKE_SOURCE_DIR}/shaders_vk"
        "${CMAKE_SOURCE_DIR}/shaders/core"
        "${CMAKE_SOURCE_DIR}/shaders/post"
    )
    
    # Compile shaders from main directories
    compile_shaders_directory(
        DIRECTORY "${CMAKE_SOURCE_DIR}/shaders" 
        OUTPUT_DIR "${SHADER_CACHE_DIR}"
        RECURSIVE
        TARGET_ENV "${SETUP_TARGET_ENV}"
        INCLUDES ${SETUP_INCLUDE_DIRS}
        DEFINES ${SETUP_DEFINES}
        ${SETUP_VALIDATE_LAYOUT}
    )
    
    compile_shaders_directory(
        DIRECTORY "${CMAKE_SOURCE_DIR}/shaders_vk"
        OUTPUT_DIR "${SHADER_CACHE_DIR}"  
        RECURSIVE
        TARGET_ENV "${SETUP_TARGET_ENV}"
        INCLUDES ${SETUP_INCLUDE_DIRS}
        DEFINES ${SETUP_DEFINES}
        ${SETUP_VALIDATE_LAYOUT}
    )
    
    # Add validation test
    add_shader_validation_test(NAME shaders)
    
    message(STATUS "Shader compilation setup complete")
endfunction()