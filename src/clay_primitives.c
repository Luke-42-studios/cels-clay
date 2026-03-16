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
 * Clay Entity Primitives - Component Registration
 *
 * Defines component ID extern variables and registration functions for the
 * 5 primitive component types. Registration uses cels_component_register()
 * with idempotent guards (register once, skip on subsequent calls).
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 */

#include "cels-clay/clay_primitives.h"
#include <cels/cels.h>

/* ============================================================================
 * Component ID Variables and Registration Functions
 * ============================================================================
 *
 * Same pattern as ClaySurfaceConfig_id/ClaySurfaceConfig_register in clay_layout.c.
 * Each component gets:
 *   - An extern entity ID (initially 0)
 *   - A register function with idempotent guard
 */

cels_entity_t ClayContainerConfig_id = 0;
void ClayContainerConfig_register(void) {
    if (ClayContainerConfig_id == 0) {
        ClayContainerConfig_id = cels_component_register("ClayContainerConfig",
            sizeof(ClayContainerConfig), CELS_ALIGNOF(ClayContainerConfig));
    }
}

cels_entity_t ClayTextConfig_id = 0;
void ClayTextConfig_register(void) {
    if (ClayTextConfig_id == 0) {
        ClayTextConfig_id = cels_component_register("ClayTextConfig",
            sizeof(ClayTextConfig), CELS_ALIGNOF(ClayTextConfig));
    }
}

cels_entity_t ClaySpacerConfig_id = 0;
void ClaySpacerConfig_register(void) {
    if (ClaySpacerConfig_id == 0) {
        ClaySpacerConfig_id = cels_component_register("ClaySpacerConfig",
            sizeof(ClaySpacerConfig), CELS_ALIGNOF(ClaySpacerConfig));
    }
}

cels_entity_t ClayImageConfig_id = 0;
void ClayImageConfig_register(void) {
    if (ClayImageConfig_id == 0) {
        ClayImageConfig_id = cels_component_register("ClayImageConfig",
            sizeof(ClayImageConfig), CELS_ALIGNOF(ClayImageConfig));
    }
}

cels_entity_t ClayBorderStyle_id = 0;
void ClayBorderStyle_register(void) {
    if (ClayBorderStyle_id == 0) {
        ClayBorderStyle_id = cels_component_register("ClayBorderStyle",
            sizeof(ClayBorderStyle), CELS_ALIGNOF(ClayBorderStyle));
    }
}
