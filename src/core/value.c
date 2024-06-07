#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "core/value.h"
#include "core/memory.h"
#include "core/object.h"

void lox_InitValueArray(ValueArray *array)
{
    array->capacity = 0;
    array->count = 0;
    array->values = NULL;
}

void lox_WriteValueArray(ValueArray *array, Value value)
{
    if (array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void lox_FreeValueArray(ValueArray *array)
{
    FREE_ARRAY(Value, array->values, array->capacity);
    lox_InitValueArray(array);
}

void lox_PrintValue(Value value)
{
    switch (value.type)
    {
    case VAL_BOOL:
        printf(AS_BOOL(value) ? "true" : "false");
        break;
    case VAL_NIL:
        printf("nil");
        break;
    case VAL_NUMBER:
        printf("%g", AS_NUMBER(value));
        break;
    case VAL_OBJ:
        lox_PrintObject(value);
        break;
    }
}

bool lox_ValuesEqual(Value a, Value b)
{
    if (a.type != b.type)
        return false;
    switch (a.type)
    {
    case VAL_BOOL:
        return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:
        return true;
    case VAL_NUMBER:
        return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ:
        return AS_OBJ(a) == AS_OBJ(b);
    default:
        return false; // Unreachable.
    }
}
