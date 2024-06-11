#include <stdlib.h>
#include <string.h>

#include "common/hashtable.h"
#include "core/memory.h"
#include "core/object.h"
#include "core/value.h"

/// @brief Initializes hash-table. Sets everything to zero/NULL.
/// @param table to initialize.
void lox_InitHashTable(HashTable *table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

/// @brief Deletes data in 'table'.
/// @param table to delete.
void lox_FreeHashTable(HashTable *table)
{
    FREE_ARRAY(Entry, table->entries, table->capacity);
    lox_InitHashTable(table);
}

static Entry *FindEntry(Entry *entries, int capacity, ObjString *key)
{
    uint32_t index = key->hash % capacity;
    Entry *tombstone = NULL;

    for (;;)
    {
        Entry *entry = &entries[index];
        if (entry->key == NULL)
        {
            if (IS_NIL(entry->value))
            {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            }
            else
            {
                // We found a tombstone.
                if (tombstone == NULL)
                    tombstone = entry;
            }
        }
        else if (entry->key == key)
        {
            // We found the key.
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

static void AdjustCapacity(HashTable *table, int capacity)
{
    // Allocate new hash-table.
    Entry *entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++)
    {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    // Insert old(used) buckets into new hash-table.
    table->count = 0;
    for (int i = 0; i < table->capacity; i++)
    {
        Entry *entry = &table->entries[i];
        if (entry->key == NULL)
            continue;

        Entry *dest = FindEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    // Free old hash-table.
    FREE_ARRAY(Entry, table->entries, table->capacity);

    // Update hash-table structure.
    table->entries = entries;
    table->capacity = capacity;
}

/// @brief Adds an entry with 'key' and 'value' to 'table'.
/// @param table to add into.
/// @param key to add.
/// @param value to add.
/// @return true if entry does not exist. False if it does.
bool lox_AddEntryHashTable(HashTable *table, ObjString *key, Value value)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
    {
        int capacity = GROW_CAPACITY(table->capacity);
        AdjustCapacity(table, capacity);
    }

    Entry *entry = FindEntry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key && IS_NIL(entry->value))
        table->count++;

    entry->key = key;
    entry->value = value;
    return is_new_key;
}

/// @brief Copies entries in 'src' that does not exist in 'dest' to 'dest'.
/// @param src is copied from.
/// @param dest is copied to.
void lox_CopyHashTable(HashTable *src, HashTable *dest)
{
    for (int i = 0; i < src->capacity; i++)
    {
        Entry *entry = &dest->entries[i];
        if (entry->key != NULL)
        {
            lox_AddEntryHashTable(dest, entry->key, entry->value);
        }
    }
}

/// @brief Retrieves a value into 'value' if there exists an element with 'key'.
///        If no elements exists with 'key', value is NULL.
/// @param table to search.
/// @param key to find.
/// @param value to return.
/// @return true if found, false if not.
bool lox_GetEntryHashTable(HashTable *table, ObjString *key, Value *value)
{
    if (table->count == 0)
        return false;

    Entry *entry = FindEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    *value = entry->value;
    return true;
}

/// @brief Removes the entry with 'key' from 'table'.
/// @param table to remove from.
/// @param key to find entry to remove.
/// @return true if found and deleted. False if not.
bool lox_RemoveEntryHashTable(HashTable *table, ObjString *key)
{
    if (table->count == 0)
        return false;

    // Find the entry.
    Entry *entry = FindEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

ObjString *lox_FindStringHashTable(HashTable *table, const char *chars, int length, uint32_t hash)
{
    if (table->count == 0)
        return NULL;

    uint32_t index = hash % table->capacity;
    for (;;)
    {
        Entry *entry = &table->entries[index];
        if (entry->key == NULL)
        {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value))
                return NULL;
        }
        else if (entry->key->length == length &&
                 entry->key->hash == hash &&
                 memcmp(entry->key->chars, chars, length) == 0)
        {
            // We found it.
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}
