#ifndef _CLOX_VM_H_
#define _CLOX_VM_H_

#include "common/common.h"
#include "core/chunk.h"
#include "core/object.h"
#include "core/value.h"
#include "common/hashtable.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
    ObjFunction *function;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef struct
{
    CallFrame frames[FRAMES_MAX];
    int frame_count;
    Value stack[STACK_MAX];
    Value *stack_top;
    Obj *objects;
    HashTable strings;
    HashTable globals;
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
InterpretResult lox_InterpretSource(const char *source);
void lox_PushStack(Value value);
Value lox_PopStack();

#endif
