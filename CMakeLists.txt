# cels-clay - Clay layout engine integration for CELS framework
# INTERFACE library: sources compile in the consumer's context
cmake_minimum_required(VERSION 3.21)

# ============================================================================
# Fetch Clay (single-header layout library)
# ============================================================================
# NOTE: We use FetchContent_Populate (NOT MakeAvailable) because Clay's
# CMakeLists.txt builds example executables requiring raylib/SDL/etc.
# Populate only fetches the source -- we create our own INTERFACE target.

include(FetchContent)

# Local development override: use a Clay checkout instead of fetching
set(CLAY_SOURCE_DIR "" CACHE PATH "Path to local Clay source (overrides FetchContent)")

if(CLAY_SOURCE_DIR)
    # Use local clone for development (e.g., ~/workspaces/libs/clay)
    set(clay_SOURCE_DIR "${CLAY_SOURCE_DIR}")
else()
    # Fetch from GitHub at pinned release tag
    FetchContent_Declare(
        clay
        GIT_REPOSITORY https://github.com/nicbarker/clay.git
        GIT_TAG        v0.14
    )
    FetchContent_GetProperties(clay)
    if(NOT clay_POPULATED)
        FetchContent_Populate(clay)
    endif()
endif()

# ============================================================================
# INTERFACE library target
# ============================================================================
# NOTE: For standalone use (outside the CELS repo), a `if(NOT TARGET cels)`
# guard with FetchContent for CELS can be added here. Currently, the consumer
# project (CELS root) provides the cels target via add_subdirectory.

add_library(cels-clay INTERFACE)
add_library(cels::clay ALIAS cels-clay)

target_sources(cels-clay INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_engine.c
)

target_include_directories(cels-clay INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${clay_SOURCE_DIR}
)

target_link_libraries(cels-clay INTERFACE
    cels
)
