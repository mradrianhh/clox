#ifndef _CLOX_CHUNK_H_
#define _CLOX_CHUNK_H_

#include "common/common.h"
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
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_PRINT,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
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
