#ifndef _CLOX_OBJECT_H_
#define _CLOX_OBJECT_H_

#include "common/common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) IsObjType(value, OBJ_STRING)
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

typedef enum
{
    OBJ_STRING,
} ObjType;

struct Obj
{
    ObjType type;
    struct Obj *next;
};

struct ObjString
{
    Obj obj;
    int length;
    char *chars;
    uint32_t hash;
};

ObjString *lox_CopyString(const char *chars, int length);
ObjString* lox_TakeString(char* chars, int length);
void lox_PrintObject(Value value);

static inline bool IsObjType(Value value, ObjType type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
