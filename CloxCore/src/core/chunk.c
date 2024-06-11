#include <stdlib.h>

#include "core/chunk.h"
#include "core/memory.h"

void lox_InitChunk(Chunk *chunk)
{
    chunk->capacity = 0;
    chunk->count = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    lox_InitValueArray(&chunk->constants);
}

void lox_WriteChunk(Chunk *chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1)
    {
        size_t old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int lox_AddConstant(Chunk *chunk, Value value)
{
    lox_WriteValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void lox_FreeChunk(Chunk *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    lox_FreeValueArray(&chunk->constants);
    lox_InitChunk(chunk);
}
