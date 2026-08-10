find_program(SHC slangc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

if(NOT SHC)
    message(FATAL_ERROR "Shader compiler not found at ${SHC}")
endif()

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SPIRV_BIN "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

function(compile_shader shader)
    get_filename_component(FILE_NAME ${shader} NAME)
    set(SPIRV_OUTPUT "${shader}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND "${SHC}" ${shader} -O3 -I${SHADER_DIR}/shared/ -target spirv -o ${SPIRV_OUTPUT}
        COMMAND_EXPAND_LISTS
        COMMENT "Compiling ${shader} to SPIR-V"
        VERBATIM
    )
    
    list(APPEND SPIRV_BINARY_FILES ${SPIRV_OUTPUT})
    set(SPIRV_BINARY_FILES ${SPIRV_BINARY_FILES} PARENT_SCOPE)
endfunction()

# Prefix Sum
compile_shader("${SHADER_DIR}/prefix-sum/prefix_merge.slang")
compile_shader("${SHADER_DIR}/prefix-sum/prefix_segments.slang")
compile_shader("${SHADER_DIR}/prefix-sum/prefix_sum.slang")

# Ray Tracing
compile_shader("${SHADER_DIR}/ray-tracing/ground_truth.slang")
compile_shader("${SHADER_DIR}/ray-tracing/primary.slang")

# Surfel Shaders
compile_shader("${SHADER_DIR}/surfels/accelerate.slang")
compile_shader("${SHADER_DIR}/surfels/composite.slang")
compile_shader("${SHADER_DIR}/surfels/count.slang")
compile_shader("${SHADER_DIR}/surfels/direct_draw.slang")
compile_shader("${SHADER_DIR}/surfels/gather.slang")
compile_shader("${SHADER_DIR}/surfels/heatmap.slang")
compile_shader("${SHADER_DIR}/surfels/merge.slang")
compile_shader("${SHADER_DIR}/surfels/recycle.slang")
compile_shader("${SHADER_DIR}/surfels/spawn.slang")

# Misc Shaders
compile_shader("${SHADER_DIR}/final.slang")

add_custom_target(compile_spirv DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(wyre compile_spirv)
