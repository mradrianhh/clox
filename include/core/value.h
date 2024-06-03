#ifndef _CLOX_VALUE_H_
#define _CLOX_VALUE_H_

#include "common.h"

typedef double Value;

typedef struct
{
    size_t capacity;
    size_t count;
    Value *values;
} ValueArray;

void lox_InitValueArray(ValueArray *array);
void lox_WriteValueArray(ValueArray *array, Value value);
void lox_FreeValueArray(ValueArray *array);
void lox_PrintValue(Value value);

#endif
