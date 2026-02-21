/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Clay Engine Module - Clay Layout Integration for CELS
 *
 * Initializes the Clay layout engine (arena allocation, error handler,
 * layout system, render bridge) and manages the Clay lifecycle within
 * the CELS framework.
 *
 * Usage:
 *   #include <cels-clay/clay_engine.h>
 *
 *   CEL_Build(App) {
 *       Clay_Engine_use(&(ClayEngineConfig){
 *           .arena_size = 0,  // 0 = use Clay_MinMemorySize() default
 *       });
 *   }
 *
 * Advanced users can initialize subsystems individually:
 *   clay_layout_use();   // Layout system only
 *   clay_render_use();   // Render bridge only
 *
 * Consumers who need Clay layout macros (CLAY(), CLAY_TEXT, etc.)
 * should also include clay.h directly -- it is on the include path
 * from CMakeLists.txt.
 */

#ifndef CELS_CLAY_ENGINE_H
#define CELS_CLAY_ENGINE_H

#include <cels/cels.h>
#include <stdint.h>
#include "cels-clay/clay_render.h"

/* ===========================================
 * Clay Engine Config
 * ===========================================
 *
 * Configuration for Clay_Engine_use().
 * Pass arena_size = 0 to use Clay_MinMemorySize() default.
 * Pass initial_width/height = 0 to defer dimensions until ClaySurface.
 */
typedef struct ClayEngineConfig {
    uint32_t arena_size;    /* Override arena capacity in bytes (0 = default) */
    float initial_width;    /* Initial layout width (0 = not set until ClaySurface) */
    float initial_height;   /* Initial layout height (0 = not set until ClaySurface) */
} ClayEngineConfig;

/* Module entity ID and init function */
extern cels_entity_t Clay_Engine;
extern void Clay_Engine_init(void);

/* Initialize Clay engine module with configuration.
 * Call inside a CEL_Build block. Idempotent -- safe to call multiple times.
 * Pass NULL to use all defaults. */
extern void Clay_Engine_use(const ClayEngineConfig* config);

/* Composable sub-modules (for advanced users who want individual pieces).
 * NOTE: These do NOT allocate the Clay arena or call Clay_Initialize --
 * the caller is responsible for Clay setup when using these directly. */
extern void clay_layout_use(void);   /* Layout system only */
extern void clay_render_use(void);   /* Render bridge only */

#endif /* CELS_CLAY_ENGINE_H */
