# WW3D2 owns the engine shader sources and their checkout-based Slang build.
# The output target/profile are configurable so the same Slang sources can be
# compiled for every graphics API used by the engine.

if(NOT TARGET slang-bootstrap)
    message(FATAL_ERROR "The checkout-based Slang build did not provide slang-bootstrap")
endif()

set(WW3D2_SHADER_SOURCE_DIR
    "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/Shaders")
set(WW3D2_DX11_SHADER_SOURCE_DIR
    "${CMAKE_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/Backend/dx11/shaders/source")
set(WW3D2_SHADER_OUTPUT_DIR
    "${CMAKE_BINARY_DIR}/ww3d2-shaders/$<CONFIG>"
    CACHE INTERNAL "Build directory for WW3D2 compiled Slang shader assets")

# DXBC/SM5 is the active GeneralsMD target. Other WW3D2 targets can select
# their Slang output without changing the shader sources or backend contracts.
set(WW3D2_SHADER_TARGET dxbc CACHE STRING "Slang shader output target for WW3D2")
set(WW3D2_SHADER_PROFILE sm_5_0 CACHE STRING "Slang shader profile for WW3D2")

set(WW3D2_SHADER_SOURCE_FILES
    "${WW3D2_SHADER_SOURCE_DIR}/WW3D2ShaderCommon.slang"
    "${WW3D2_DX11_SHADER_SOURCE_DIR}/WW3D2PixelShader.slang"
    "${WW3D2_DX11_SHADER_SOURCE_DIR}/WW3D2VertexShader.slang"
    "${WW3D2_DX11_SHADER_SOURCE_DIR}/DefaultVertexShader.slang"
    "${WW3D2_DX11_SHADER_SOURCE_DIR}/ProcessVertices.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/invmonochrome.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/monochrome.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/motionblur.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/ocean.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/profiler_swizzle.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/roadnoise2.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/terrain.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_surface.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_displacement.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_sky.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_track.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/Trees.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/Trees.vs.slang"
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/MotionBlur.vs.slang"
)

set(WW3D2_SHADER_OUTPUTS)
function(add_ww3d2_shader source_file output_name shader_stage)
    set(output_file "${WW3D2_SHADER_OUTPUT_DIR}/Shaders/${output_name}")
    set(shader_entry main)
    set(shader_defines)
    if(ARGC GREATER 3)
        if(ARGV3 STREQUAL "ENTRY" AND ARGC GREATER 4)
            set(shader_entry "${ARGV4}")
        else()
            list(APPEND shader_defines "-D${ARGV3}")
        endif()
    endif()
    add_custom_command(
        OUTPUT "${output_file}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${WW3D2_SHADER_OUTPUT_DIR}/Shaders"
        COMMAND $<TARGET_FILE:slang-bootstrap>
            -target "${WW3D2_SHADER_TARGET}"
            -profile "${WW3D2_SHADER_PROFILE}"
            -entry "${shader_entry}"
            -stage "${shader_stage}"
            -matrix-layout-row-major
            ${shader_defines}
            -I "${WW3D2_SHADER_SOURCE_DIR}"
            -I "${WW3D2_DX11_SHADER_SOURCE_DIR}"
            -o "${output_file}"
            "${source_file}"
        DEPENDS "${source_file}"
            "${WW3D2_SHADER_SOURCE_DIR}/WW3D2ShaderCommon.slang"
            slang-bootstrap
        COMMENT "Compiling WW3D2 Slang shader ${output_name}"
        VERBATIM
    )
    set(WW3D2_SHADER_OUTPUTS
        ${WW3D2_SHADER_OUTPUTS} "${output_file}" PARENT_SCOPE)
endfunction()

add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/invmonochrome.slang" invmonochrome.pso pixel)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/monochrome.slang" monochrome.pso pixel)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/motionblur.slang" motionblur.pso pixel)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/profiler_swizzle.slang" profiler_swizzle.pso pixel)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/roadnoise2.slang" roadnoise2.pso pixel)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/terrain.slang" terrain.pso pixel ENTRY TerrainPixelMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/terrain.slang" terrain.vso vertex ENTRY TerrainVertexMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/ocean.slang" ocean.pso pixel ENTRY OceanPixelMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/ocean.slang" ocean.vso vertex ENTRY OceanVertexMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_displacement.slang" water_displacement.pso pixel ENTRY WaterDisplacementPixelMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_displacement.slang" water_displacement.vso vertex ENTRY WaterDisplacementVertexMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_surface.slang" water_surface.pso pixel ENTRY WaterSurfacePixelMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_surface.slang" water_surface.vso vertex ENTRY WaterSurfaceVertexMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_sky.slang" water_sky.pso pixel ENTRY WaterSkyPixelMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_sky.slang" water_sky.vso vertex ENTRY WaterSkyVertexMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_track.slang" water_track.pso pixel ENTRY WaterTrackPixelMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/water_track.slang" water_track.vso vertex ENTRY WaterTrackVertexMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/Trees.slang" Trees.pso pixel ENTRY TreePixelMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/Trees.vs.slang" Trees.vso vertex ENTRY TreeVertexMain)
add_ww3d2_shader(
    "${WW3D2_SHADER_SOURCE_DIR}/Engine/MotionBlur.vs.slang" MotionBlur.vso vertex)
add_ww3d2_shader(
    "${WW3D2_DX11_SHADER_SOURCE_DIR}/WW3D2PixelShader.slang" standard.pso pixel)

add_ww3d2_shader(
    "${WW3D2_DX11_SHADER_SOURCE_DIR}/ProcessVertices.slang" process_vertices.cso compute)

# A cube/volume SRV requires a matching shader resource declaration. Compile a
# concrete variant for every possible stage once at build time.
foreach(WW3D2_TEXTURE_STAGE_INDEX RANGE 0 7)
    add_ww3d2_shader(
        "${WW3D2_DX11_SHADER_SOURCE_DIR}/WW3D2PixelShader.slang"
        "standard_cube_${WW3D2_TEXTURE_STAGE_INDEX}.pso"
        pixel
        "WW3D2_TEXTURE${WW3D2_TEXTURE_STAGE_INDEX}_TYPE=1")
    add_ww3d2_shader(
        "${WW3D2_DX11_SHADER_SOURCE_DIR}/WW3D2PixelShader.slang"
        "standard_volume_${WW3D2_TEXTURE_STAGE_INDEX}.pso"
        pixel
        "WW3D2_TEXTURE${WW3D2_TEXTURE_STAGE_INDEX}_TYPE=2")
endforeach()

# Compile every neutral vertex input signature, also at build time.
foreach(WW3D2_VERTEX_FORMAT_INDEX RANGE 1 21)
    add_ww3d2_shader(
        "${WW3D2_DX11_SHADER_SOURCE_DIR}/DefaultVertexShader.slang"
        "standard_vertex_${WW3D2_VERTEX_FORMAT_INDEX}.vso"
        vertex
        "WW3D2_VERTEX_FORMAT=${WW3D2_VERTEX_FORMAT_INDEX}")
endforeach()

add_custom_target(ww3d2_shaders DEPENDS ${WW3D2_SHADER_OUTPUTS})
