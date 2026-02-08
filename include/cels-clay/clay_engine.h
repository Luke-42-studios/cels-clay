/*
 * Clay Engine Module - Clay Layout Integration for CELS
 *
 * Initializes the Clay layout engine (arena allocation, error handler)
 * and manages the Clay lifecycle within the CELS framework.
 *
 * Usage:
 *   #include <cels-clay/clay_engine.h>
 *
 *   CEL_Build(App) {
 *       Clay_Engine_use((Clay_EngineConfig){
 *           .arena_size = 0,  // 0 = use Clay_MinMemorySize() default
 *       });
 *   }
 *
 * Consumers who need Clay layout macros (CLAY(), CLAY_TEXT, etc.)
 * should also include clay.h directly -- it is on the include path
 * from CMakeLists.txt.
 */

#ifndef CELS_CLAY_ENGINE_H
#define CELS_CLAY_ENGINE_H

#include <cels/cels.h>
#include <stdint.h>

/* ===========================================
 * Clay Engine Config
 * ===========================================
 *
 * Configuration for Clay_Engine_use().
 * Pass arena_size = 0 to use Clay_MinMemorySize() default.
 */
typedef struct Clay_EngineConfig {
    uint32_t arena_size;    /* Override arena capacity in bytes (0 = default) */
} Clay_EngineConfig;

/* Module entity ID and init function */
extern cels_entity_t Clay_Engine;
extern void Clay_Engine_init(void);

/* Initialize Clay engine module with configuration.
 * Call inside a CEL_Build block. Idempotent -- safe to call multiple times. */
extern void Clay_Engine_use(Clay_EngineConfig config);

#endif /* CELS_CLAY_ENGINE_H */
