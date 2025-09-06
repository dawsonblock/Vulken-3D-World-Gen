# Requires: GLSLC_EXECUTABLE discovered (see below)
# Compiles all GLSL variants to SPIR-V under ${CMAKE_BINARY_DIR}/shaders

file(GLOB_RECURSE GLSL_SOURCES
    "${CMAKE_SOURCE_DIR}/shaders_vk/*.vert"
    "${CMAKE_SOURCE_DIR}/shaders_vk/*.frag"
    "${CMAKE_SOURCE_DIR}/shaders_vk/*.comp"
    "${CMAKE_SOURCE_DIR}/shaders_vk/*.geom"
    "${CMAKE_SOURCE_DIR}/shaders_vk/*.tesc"
    "${CMAKE_SOURCE_DIR}/shaders_vk/*.tese"
    "${CMAKE_SOURCE_DIR}/shaders_vk/*.glsl"
)

set(SPV_OUTPUTS "")
foreach(GLSL ${GLSL_SOURCES})
    # mirror relative path under build/shaders
    file(RELATIVE_PATH REL ${CMAKE_SOURCE_DIR}/shaders_vk ${GLSL})
    get_filename_component(DIR "${REL}" DIRECTORY)
    get_filename_component(FN "${GLSL}" NAME)
    set(OUTDIR "${CMAKE_BINARY_DIR}/shaders/${DIR}")
    file(MAKE_DIRECTORY "${OUTDIR}")
    set(SPV "${OUTDIR}/${FN}.spv")

    add_custom_command(
        OUTPUT "${SPV}"
        COMMAND "${GLSLC_EXECUTABLE}" -O -c "${GLSL}" -o "${SPV}"
        DEPENDS "${GLSL}"
        COMMENT "Compiling shader ${REL}"
        VERBATIM
    )
    list(APPEND SPV_OUTPUTS "${SPV}")
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPV_OUTPUTS})
install(DIRECTORY "${CMAKE_BINARY_DIR}/shaders/" DESTINATION shaders)
