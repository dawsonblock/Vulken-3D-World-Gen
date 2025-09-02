file(GLOB_RECURSE GLSL_SRC
  "${CMAKE_SOURCE_DIR}/shaders/*.vert"
  "${CMAKE_SOURCE_DIR}/shaders/*.frag"
  "${CMAKE_SOURCE_DIR}/shaders/*.comp"
  "${CMAKE_SOURCE_DIR}/shaders_vk/*.vert"
  "${CMAKE_SOURCE_DIR}/shaders_vk/*.frag"
  "${CMAKE_SOURCE_DIR}/shaders_vk/*.comp"
  "${CMAKE_SOURCE_DIR}/shaders_vk/*.glsl")

set(SPV_DIR "${CMAKE_BINARY_DIR}/.cache/spv")
set(SPV_OUTPUTS)

find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC)
  find_program(GLSLANG_VALIDATOR glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
  if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "Neither glslc nor glslangValidator found. Install Vulkan SDK or shaderc tools.")
  endif()
  set(GLSLC ${GLSLANG_VALIDATOR})
  set(GLSLC_FLAGS -V --target-env vulkan1.3)
else()
  set(GLSLC_FLAGS --target-env=vulkan1.3 -O)
endif()

message(STATUS "Shader compiler: ${GLSLC}")
message(STATUS "Found ${CMAKE_MATCH_COUNT} shader files")

foreach(src ${GLSL_SRC})
  get_filename_component(name ${src} NAME)
  file(RELATIVE_PATH rel_path "${CMAKE_SOURCE_DIR}" "${src}")
  get_filename_component(rel_dir "${rel_path}" DIRECTORY)
  
  set(out_dir "${SPV_DIR}/${rel_dir}")
  set(out "${out_dir}/${name}.spv")
  
  # Determine shader stage based on filename or content
  set(SHADER_STAGE_FLAGS "")
  if(name MATCHES "\\.vert\\." OR name MATCHES "\\.vert$")
    set(SHADER_STAGE_FLAGS "-fshader-stage=vertex")
  elseif(name MATCHES "\\.frag\\." OR name MATCHES "\\.frag$")
    set(SHADER_STAGE_FLAGS "-fshader-stage=fragment")
  elseif(name MATCHES "\\.comp\\." OR name MATCHES "\\.comp$")
    set(SHADER_STAGE_FLAGS "-fshader-stage=compute")
  elseif(name MATCHES "\\.geom\\." OR name MATCHES "\\.geom$")
    set(SHADER_STAGE_FLAGS "-fshader-stage=geometry")
  elseif(name MATCHES "\\.tesc\\." OR name MATCHES "\\.tesc$")
    set(SHADER_STAGE_FLAGS "-fshader-stage=tesscontrol")
  elseif(name MATCHES "\\.tese\\." OR name MATCHES "\\.tese$")
    set(SHADER_STAGE_FLAGS "-fshader-stage=tesseval")
  elseif(name MATCHES "\\.glsl$")
    # Skip .glsl files as they are typically include files
    continue()
  endif()
  
  add_custom_command(
    OUTPUT ${out}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
    COMMAND ${GLSLC} ${GLSLC_FLAGS} ${SHADER_STAGE_FLAGS} ${src} -o ${out}
    DEPENDS ${src}
    COMMENT "Compiling shader: ${rel_path}")
  list(APPEND SPV_OUTPUTS ${out})
endforeach()

if(SPV_OUTPUTS)
  add_custom_target(ShaderSPV ALL DEPENDS ${SPV_OUTPUTS})
  message(STATUS "ShaderSPV target will compile ${CMAKE_MATCH_COUNT} shaders")
else()
  add_custom_target(ShaderSPV)
  message(WARNING "No shaders found for compilation")
endif()