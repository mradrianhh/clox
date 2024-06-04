#ifndef _CLOX_MEMORY_H_
#define _CLOX_MEMORY_H_

#include "common.h"
#include "core/object.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, old_count, new_count)         \
    (type *)lox_Reallocate(pointer, sizeof(type) * (old_count), \
                           sizeof(type) * (new_count))

#define FREE_ARRAY(type, pointer, old_count) \
    lox_Reallocate(pointer, sizeof(type) * (old_count), 0)

#define FREE(type, pointer) lox_Reallocate(pointer, sizeof(type), 0)

#define ALLOCATE(type, count) \
    (type *)lox_Reallocate(NULL, 0, sizeof(type) * (count))

void *lox_Reallocate(void *pointer, size_t old_size, size_t new_size);
void lox_FreeObjects();

#endif
