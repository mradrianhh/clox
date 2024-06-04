#include <stdio.h>
#include <string.h>

#include "core/memory.h"
#include "core/object.h"
#include "core/value.h"
#include "vm/vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type *)AllocateObject(sizeof(type), objectType)

static Obj *AllocateObject(size_t size, ObjType type)
{
    Obj *object = (Obj *)lox_Reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

static ObjString *AllocateString(char *chars, int length)
{
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString *lox_CopyString(const char *chars, int length)
{
    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return AllocateString(heapChars, length);
}

ObjString *lox_TakeString(char *chars, int length)
{
    return AllocateString(chars, length);
}

void lox_PrintObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    }
}
