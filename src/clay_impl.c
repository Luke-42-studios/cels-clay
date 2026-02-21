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
