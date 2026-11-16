// Host stand-in for ESP-IDF's esp_heap_caps.h: PSRAM is just the heap here.
#pragma once

#include <stdlib.h>

#define MALLOC_CAP_SPIRAM 0

static inline void *heap_caps_aligned_alloc(size_t alignment, size_t size, unsigned caps)
{
    void *p = NULL;
    (void)caps;
    return posix_memalign(&p, alignment, size) == 0 ? p : NULL;
}
