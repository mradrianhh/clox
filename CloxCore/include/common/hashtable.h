#ifndef _CLOX_HASH_TABLE_H_
#define _CLOX_HASH_TABLE_H_

#include "common/common.h"
#include "core/value.h"

#define TABLE_MAX_LOAD 0.75

typedef struct
{
    ObjString *key;
    Value value;
} Entry;

typedef struct
{
    size_t count;
    size_t capacity;
    Entry *entries;
} HashTable;

void lox_InitHashTable(HashTable *table);
void lox_FreeHashTable(HashTable *table);
bool lox_AddEntryHashTable(HashTable *table, ObjString *key, Value value);
void lox_CopyHashTable(HashTable *src, HashTable *dest);
bool lox_GetEntryHashTable(HashTable *table, ObjString *key, Value *value);
bool lox_RemoveEntryHashTable(HashTable* table, ObjString* key);
ObjString *lox_FindStringHashTable(HashTable *table, const char *chars, int length, uint32_t hash);

#endif
