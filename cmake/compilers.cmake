
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

# Add debug symbols to Release builds for crash dump analysis, profiling, and
# post-mortem debugging.
string(APPEND CMAKE_CXX_FLAGS_RELEASE " -g")
string(APPEND CMAKE_C_FLAGS_RELEASE " -g")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Ensures only ISO features are used

# Recover cross-module inlining in the optimized game and graphics libraries.
# Debug builds retain normal compilation for short edit/build cycles.
option(RTS_ENABLE_IPO "Enable link-time optimization in optimized builds" ON)
if(RTS_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT rts_ipo_supported OUTPUT rts_ipo_error LANGUAGES C CXX)
    if(NOT rts_ipo_supported)
        message(FATAL_ERROR "Link-time optimization is unavailable: ${rts_ipo_error}")
    endif()
endif()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ${RTS_ENABLE_IPO})
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ${RTS_ENABLE_IPO})

add_compile_options(-Wsuggest-override)

if(RTS_BUILD_OPTION_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()
