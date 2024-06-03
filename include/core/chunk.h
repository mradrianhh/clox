#ifndef _CLOX_CHUNK_H_
#define _CLOX_CHUNK_H_

#include "common.h"
#include "core/value.h"

typedef enum 
{
    OP_CONSTANT,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_RETURN,
} Opcode;

typedef struct 
{
    size_t capacity;
    size_t count;
    uint8_t *code;
    int *lines;
    ValueArray constants;
} Chunk;

void lox_InitChunk(Chunk *chunk);
void lox_WriteChunk(Chunk *chunk, uint8_t byte, int line);
int lox_AddConstant(Chunk *chunk, Value value);
void lox_FreeChunk(Chunk *chunk);

#endif
