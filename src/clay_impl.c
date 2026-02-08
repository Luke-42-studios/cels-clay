/*
 * Clay Implementation Translation Unit
 *
 * Clay is a single-header library. Defining CLAY_IMPLEMENTATION before
 * including clay.h instantiates all of Clay's function implementations.
 * This MUST happen in exactly ONE .c file -- defining it in multiple
 * translation units causes duplicate symbol linker errors.
 *
 * All other source files that need Clay should include clay.h WITHOUT
 * defining CLAY_IMPLEMENTATION (they get declarations only).
 */
#define CLAY_IMPLEMENTATION
#include "clay.h"
