# Phase 1: Build System + Clay Initialization - Context

**Gathered:** 2026-02-07
**Status:** Ready for planning

<domain>
## Phase Boundary

CMake integration to fetch the Clay layout library, compile it into a linkable target, allocate and initialize the Clay arena, and wire an error handler. This is pure build/init infrastructure — no layout logic, no rendering, no user-facing API beyond module startup.

</domain>

<decisions>
## Implementation Decisions

### Clay fetching strategy
- Use CMake FetchContent to download Clay from GitHub (consistent with how CELS fetches flecs/yyjson)
- Pin to a specific release tag for deterministic builds
- cels-clay handles CLAY_IMPLEMENTATION internally — consumer just links the library, no need to define it themselves
- Provide a CMake option (e.g. CLAY_SOURCE_DIR) to use a local Clay clone during development, upstream fetch by default

### Arena configuration
- Arena size is configurable with a sensible default (consumer can override via config struct or CMake option)
- Arena memory allocated via malloc — Clay owns one block, freed on shutdown
- Clay_Initialize runs at module startup (when Clay_Engine_use() is called), before any frame ticks
- Arena cleanup is automatic when the CELS world is destroyed — register a cleanup callback, no explicit shutdown call needed

### Error handler behavior
- Clay errors (duplicate IDs, capacity exceeded, etc.) go to stderr via fprintf
- No custom error handler callback — stderr only, keep it simple
- Error messages are descriptive with context: include error type, element ID, and human-readable description (e.g. "Clay error: duplicate ID 'sidebar' in layout")
- All errors are warnings — log and continue, never abort. Let the app keep running even with layout issues

### Build integration surface
- Consumer projects integrate via add_subdirectory (or FetchContent) — matches cels-ncurses pattern
- Clay headers are exposed as PUBLIC — consumers can use CLAY() macros directly (needed for CEL_Clay() API in Phase 2)
- CMake target name: `cels::clay` (namespaced, modern CMake style)
- cels-clay fetches CELS via FetchContent if not already available — works both as part of a CELS project and standalone

### Claude's Discretion
- Exact default arena size
- CMake variable naming conventions beyond what's specified
- Internal file organization (source layout within the module)
- Clay compile flags and optimization settings

</decisions>

<specifics>
## Specific Ideas

- Local override option mirrors development workflow — user has a Clay fork at ~/workspaces/libs/clay that they may want to iterate on alongside cels-clay
- FetchContent + release tag pinning follows the established CELS pattern (flecs, yyjson)
- The `cels::clay` namespace target follows modern CMake best practices

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-build-system-clay-initialization*
*Context gathered: 2026-02-07*
