#include <stdlib.h>
#include <stdio.h>

#include "core/value.h"
#include "core/memory.h"

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
    printf("%g", value);
}
