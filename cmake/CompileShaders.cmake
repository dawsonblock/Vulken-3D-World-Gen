# Only compile shaders when graphics is enabled
# TODO: Fix shader compilation system to properly handle include files
if(FALSE AND ENABLE_GRAPHICS)
  # Find glslc
  find_program(GLSLC glslc
    HINTS
      ENV VULKAN_SDK
      ENV VULKAN_SDK_PATH
    PATH_SUFFIXES
      Bin
      bin
    REQUIRED
  )

  if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Please install Vulkan SDK or set VULKAN_SDK environment variable.")
  endif()

  message(STATUS "Using glslc: ${GLSLC}")

  # Set up shader compilation
  set(SPV_DIR "${CMAKE_BINARY_DIR}/.cache/spv")
  set(SPV_OUTPUTS)

  # Only compile actual shader files with standard extensions
  file(GLOB_RECURSE ACTUAL_SHADERS
    "${CMAKE_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_SOURCE_DIR}/shaders/*.comp"
    "${CMAKE_SOURCE_DIR}/shaders/*.geom"
    "${CMAKE_SOURCE_DIR}/shaders/*.tesc"
    "${CMAKE_SOURCE_DIR}/shaders/*.tese"
    "${CMAKE_SOURCE_DIR}/shaders/*.mesh"
    "${CMAKE_SOURCE_DIR}/shaders/*.task"
    "${CMAKE_SOURCE_DIR}/shaders/*.rgen"
    "${CMAKE_SOURCE_DIR}/shaders/*.rchit"
    "${CMAKE_SOURCE_DIR}/shaders/*.rmiss"
    "${CMAKE_SOURCE_DIR}/shaders/*.rahit"
    "${CMAKE_SOURCE_DIR}/shaders/*.rcall"
    "${CMAKE_SOURCE_DIR}/shaders/*.rint"
  )

  # Exclude include files (files without main() function)
  set(EXCLUDE_FILES
    "${CMAKE_SOURCE_DIR}/shaders_vk/shadows/csm_common.glsl"
    "${CMAKE_SOURCE_DIR}/shaders_vk/shadows/csm_debug.glsl"
    "${CMAKE_SOURCE_DIR}/shaders_vk/common/weather_ubo.glsl"
    "${CMAKE_SOURCE_DIR}/shaders_vk/lighting/pbr_common.glsl"
    "${CMAKE_SOURCE_DIR}/shaders_vk/lighting/pcss.glsl"
    "${CMAKE_SOURCE_DIR}/shaders_vk/lighting/csm_binding.glsl"
    "${CMAKE_SOURCE_DIR}/shaders_vk/material/weather_material.glsl"
  )

  # Compile each shader
  foreach(src ${ACTUAL_SHADERS})
    if(NOT EXISTS ${src})
      continue()
    endif()

    # Skip exclude files
    if(src IN_LIST EXCLUDE_FILES)
      continue()
    endif()

    get_filename_component(name ${src} NAME_WE)
    get_filename_component(rel_path ${src} RELATIVE ${CMAKE_SOURCE_DIR})
    get_filename_component(rel_dir ${rel_path} DIRECTORY)

    set(out_dir "${SPV_DIR}/${rel_dir}")
    set(out "${out_dir}/${name}.spv")

    # Determine shader stage based on filename
    set(SHADER_STAGE_FLAGS "")
    if(name MATCHES "\\.vert$" OR name MATCHES "vert\\.glsl$")
      set(SHADER_STAGE_FLAGS "-fshader-stage=vertex")
    elseif(name MATCHES "\\.frag$" OR name MATCHES "frag\\.glsl$")
      set(SHADER_STAGE_FLAGS "-fshader-stage=fragment")
    elseif(name MATCHES "\\.comp$" OR name MATCHES "comp\\.glsl$")
      set(SHADER_STAGE_FLAGS "-fshader-stage=compute")
    elseif(name MATCHES "\\.geom$" OR name MATCHES "geom\\.glsl$")
      set(SHADER_STAGE_FLAGS "-fshader-stage=geometry")
    elseif(name MATCHES "\\.tesc$" OR name MATCHES "tesc\\.glsl$")
      set(SHADER_STAGE_FLAGS "-fshader-stage=tesscontrol")
    elseif(name MATCHES "\\.tese$" OR name MATCHES "tese\\.glsl$")
      set(SHADER_STAGE_FLAGS "-fshader-stage=tesseval")
    else()
      # Skip files that don't match known shader patterns
      continue()
    endif()

    add_custom_command(
      OUTPUT ${out}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
      COMMAND ${GLSLC} ${SHADER_STAGE_FLAGS} ${src} -o ${out}
      DEPENDS ${src}
      COMMENT "Compiling shader ${rel_path}"
      VERBATIM
    )

    list(APPEND SPV_OUTPUTS ${out})
  endforeach()

  # Create the shader compilation target
  add_custom_target(compile_shaders ALL DEPENDS ${SPV_OUTPUTS})

  # Add shader directory to include path
  target_include_directories(VoxelVK_Elite_ALL PRIVATE ${SPV_DIR})

else()
  # Create dummy target when graphics is disabled
  add_custom_target(compile_shaders)
endif()
