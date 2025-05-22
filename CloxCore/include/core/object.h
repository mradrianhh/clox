#ifndef _CLOX_OBJECT_H_
#define _CLOX_OBJECT_H_

#include "common/common.h"
#include "value.h"
#include "chunk.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLOSURE(value) IsObjType(value, OBJ_CLOSURE)
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))

#define IS_FUNCTION(value) IsObjType(value, OBJ_FUNCTION)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))

#define IS_NATIVE(value) IsObjType(value, OBJ_NATIVE)
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)

#define IS_STRING(value) IsObjType(value, OBJ_STRING)
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

typedef enum
{
    OBJ_STRING,
    // Function is the compile-time representation of a function.
    OBJ_FUNCTION,
    // Closure is the runtime representation of a function.
    OBJ_CLOSURE,
    OBJ_NATIVE,
} ObjType;

struct Obj
{
    ObjType type;
    struct Obj *next;
};

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct
{
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjString
{
    Obj obj;
    int length;
    char *chars;
    uint32_t hash;
};

// ObjFunction is the compile-time representation of a function.
typedef struct
{
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

// ObjClosure is a wrapped around ObjFunction providing the runtime-representation of a function.
// This provides all the runtime-state necessary.
typedef struct
{
    Obj obj;
    ObjFunction *function;
} ObjClosure;
 
ObjClosure *lox_CreateClosure(ObjFunction *function);
ObjFunction *lox_CreateFunction();
ObjNative *lox_CreateNative(NativeFn function);
ObjString *lox_CopyString(const char *chars, int length);
ObjString *lox_TakeString(char *chars, int length);
void lox_PrintObject(Value value);

static inline bool IsObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
