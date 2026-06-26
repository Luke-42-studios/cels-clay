# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# cels-clay - Clay layout engine integration for CELS framework
# Four INTERFACE targets: cels-clay-core, cels-clay-sdl3, cels-clay-ncurses, cels-clay-widgets
# Plus aggregate cels-clay alias for back-compat.
cmake_minimum_required(VERSION 3.21)
project(cels-clay VERSION 0.7.0 LANGUAGES C)

# Vendor CPM — shared dependency cache: export CPM_SOURCE_CACHE=$HOME/.cache/CPM
if(NOT COMMAND CPMAddPackage)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)
endif()

# ============================================================================
# Fetch Clay (single-header layout library)
# ============================================================================
# Clay is header-only. Fast path: sibling ../clay/clay.h auto-detect (dev
# layout). Fallback: CPM DOWNLOAD_ONLY so Clay's own CMake/examples (raylib,
# SDL) never execute — we create our own INTERFACE target below.
# Local-dev override (replaces CLAY_SOURCE_DIR cache var):
#   cmake -S . -B build -DCLAY_SOURCE_DIR=/path/to/clay
#   cmake -S . -B build -DCPM_cels_SOURCE=../cels -DCPM_cels-theme_SOURCE=../cels-theme
# Shared dep cache:
#   export CPM_SOURCE_CACHE=$HOME/.cache/CPM

# Local development override: use a Clay checkout instead of CPM fetch
set(CLAY_SOURCE_DIR "" CACHE PATH "Path to local Clay source (overrides CPM fetch)")

# Auto-detect sibling clay directory (fast path — no network fetch)
if(NOT CLAY_SOURCE_DIR AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../clay/clay.h)
    set(CLAY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../clay" CACHE PATH "" FORCE)
endif()

if(CLAY_SOURCE_DIR)
    # Fast path: use local clone for development (e.g., ~/workspaces/libs/clay)
    set(clay_SOURCE_DIR "${CLAY_SOURCE_DIR}")
else()
    # Fallback: fetch from GitHub at pinned release tag (header-only, DOWNLOAD_ONLY)
    # CPM sets clay_SOURCE_DIR after fetch; Clay's CMakeLists.txt is never run.
    CPMAddPackage(
        NAME clay
        GITHUB_REPOSITORY nicbarker/clay
        GIT_TAG v0.14
        OPTIONS "CLAY_INCLUDE_ALL_EXAMPLES OFF"
        DOWNLOAD_ONLY YES
    )
    # clay_SOURCE_DIR is populated by CPM after the DOWNLOAD_ONLY fetch above
endif()

# ============================================================================
# Sibling dependencies (standalone build support)
# ============================================================================
# When built standalone, CPM fetches pinned tags. When included via
# add_subdirectory or CPM from a parent project, these targets already exist
# and the guards skip (CPMAddPackage is idempotent by NAME).
# Local-dev sibling override: -DCPM_cels_SOURCE=../cels -DCPM_cels-theme_SOURCE=../cels-theme

if(NOT TARGET cels)
    CPMAddPackage(
        NAME cels
        GITHUB_REPOSITORY Luke-42-studios/cels
        GIT_TAG v0.6.1
    )
endif()

if(NOT TARGET cels-theme)
    CPMAddPackage(
        NAME cels-theme
        GITHUB_REPOSITORY Luke-42-studios/cels-theme
        GIT_TAG v0.1.0
    )
endif()

# ============================================================================
# cels-clay-core — layout engine, no renderer deps
# ============================================================================
# Links only cels + cels-theme. No SDL3, ttf, image, or ncurses.
# cels-theme provides cels_color_t which resolves the former nucleus_types.h
# coupling (that cross-repo include path is now deleted).

add_library(cels-clay-core INTERFACE)
target_sources(cels-clay-core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_layout.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_render.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_primitives.c
)
target_include_directories(cels-clay-core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${clay_SOURCE_DIR}
)
target_link_libraries(cels-clay-core INTERFACE cels cels-theme)

# ============================================================================
# cels-clay-sdl3 — SDL3 renderer
# ============================================================================
# Links cels-clay-core + cels-sdl3. Headers under include/cels-clay-sdl3/ so
# they only resolve when this target is linked (per-target include path).

add_library(cels-clay-sdl3 INTERFACE)
target_sources(cels-clay-sdl3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_sdl3_renderer.c
)
target_include_directories(cels-clay-sdl3 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cels-clay-sdl3
)
target_link_libraries(cels-clay-sdl3 INTERFACE cels-clay-core cels-sdl3)

# ============================================================================
# cels-clay-ncurses — ncurses renderer
# ============================================================================
# Links cels-clay-core + cels-ncurses. Headers under include/cels-clay-ncurses/.

add_library(cels-clay-ncurses INTERFACE)
target_sources(cels-clay-ncurses INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_ncurses_renderer.c
)
target_include_directories(cels-clay-ncurses INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cels-clay-ncurses
)
target_link_libraries(cels-clay-ncurses INTERFACE cels-clay-core cels-ncurses)

# ============================================================================
# cels-clay-widgets — UI widget layer
# ============================================================================
# clay_button.c is created in plan 03-03; the target is declared here so the
# target name resolves for Nucleus link lines. Headers under include/cels-clay-widgets/.

add_library(cels-clay-widgets INTERFACE)
target_sources(cels-clay-widgets INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/clay_button.c
)
target_include_directories(cels-clay-widgets INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cels-clay-widgets
)
target_link_libraries(cels-clay-widgets INTERFACE cels-clay-core)

# ============================================================================
# Back-compat aggregate alias (D-14)
# ============================================================================
# cels-clay pulls in all four per-target libs. Existing consumers that link
# cels-clay continue to build unchanged during the deprecation window.

add_library(cels-clay INTERFACE)
target_link_libraries(cels-clay INTERFACE
    cels-clay-core
    cels-clay-sdl3
    cels-clay-ncurses
    cels-clay-widgets
)
add_library(cels::clay ALIAS cels-clay)

# ============================================================================
# Example applications (optional)
# ============================================================================
option(CELS_CLAY_BUILD_EXAMPLES "Build cels-clay example applications" ON)

if(CELS_CLAY_BUILD_EXAMPLES)
    # NCurses example
    if(TARGET cels-ncurses)
        add_executable(main_ncurses
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal/main_ncurses.c
        )
        target_link_libraries(main_ncurses PRIVATE
            cels-clay-core
            cels-clay-ncurses
        )
        target_include_directories(main_ncurses PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal
        )
    endif()

    # SDL3 example
    if(TARGET cels-sdl3)
        add_executable(main_sdl3
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal/main_sdl3.c
        )
        target_link_libraries(main_sdl3 PRIVATE
            cels-clay-core
            cels-clay-sdl3
        )
        target_include_directories(main_sdl3 PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/minimal
        )
    endif()
endif()
