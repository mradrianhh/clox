#ifndef _CLOX_VM_H_
#define _CLOX_VM_H_

#include "common.h"
#include "core/chunk.h"
#include "core/value.h"

#define STACK_MAX 256

typedef struct
{
    Chunk *chunk;
    uint8_t *ip;
    Value stack[STACK_MAX];
    Value *stack_top;
    Obj *objects;
} VM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void lox_InitVM();
void lox_FreeVM();
InterpretResult lox_InterpretChunk(Chunk *chunk);
InterpretResult lox_InterpretSource(const char *source);
void lox_PushStack(Value value);
Value lox_PopStack();

#endif
