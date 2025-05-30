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

static ObjString *AllocateString(char *chars, int length, uint32_t hash)
{
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    lox_AddEntryHashTable(&vm.strings, string, NIL_VAL);
    return string;
}

/// @brief Hashes a string using FNV-1a.
/// @param string to hash.
/// @param length of string to hash.
/// @return the hash.
static uint32_t HashString(const char *string, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)string[i];
        hash *= 16777619;
    }
    return hash;
}

ObjClosure *lox_CreateClosure(ObjFunction *function)
{
    ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    return closure;
}

ObjFunction *lox_CreateFunction()
{
    ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    lox_InitChunk(&function->chunk);
    return function;
}

ObjNative *lox_CreateNative(NativeFn function)
{
    ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

ObjString *lox_CopyString(const char *chars, int length)
{
    uint32_t hash = HashString(chars, length);

    // Check if string is interned.
    ObjString *interned = lox_FindStringHashTable(&vm.strings, chars, length, hash);
    if (interned != NULL)
        return interned;

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return AllocateString(heapChars, length, hash);
}

ObjString *lox_TakeString(char *chars, int length)
{
    uint32_t hash = HashString(chars, length);

    // Check if string is interned.
    ObjString *interned = lox_FindStringHashTable(&vm.strings, chars, length, hash);
    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return AllocateString(chars, length, hash);
}

static void PrintFunction(ObjFunction *function)
{
    if (function->name == NULL)
    {
        printf("<script>");
        return;
    }

    printf("<fn %s>", function->name->chars);
}

void lox_PrintObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJ_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    case OBJ_FUNCTION:
        PrintFunction(AS_FUNCTION(value));
        break;
    case OBJ_CLOSURE:
        PrintFunction(AS_CLOSURE(value)->function);
        break;
    case OBJ_NATIVE:
        printf("<native fn>");
        break;
    }
}
