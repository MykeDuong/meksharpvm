#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.5

void initTable(Table* table, VirtualMachine* vm, Compiler* compiler) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table* table, VirtualMachine* vm, Compiler* compiler) {
  FREE_ARRAY(Entry, table->entries, table->capacity, vm, compiler);
  initTable(table, vm, compiler);
}

static Entry* findEntry(Entry* entries, int capacity, Value key) {
  uint32_t index = hashValue(key) % capacity;
  Entry* tombstone = NULL;
  for (;;) {
    Entry* entry = &entries[index];
    
    if(IS_EMPTY(entry->key)) {
      if (IS_NAH(entry->value)) {
        // Empty entry
        return tombstone != NULL ? tombstone : entry;
      } else {
        // If found tombstone 
        if (tombstone == NULL) tombstone = entry;
      }
    } else if (valuesEqual(key, entry->key)) {
      // Found the key 
      return entry;
    }
      
    index = (index + 1) % capacity;
  }
}

static void adjustCapacity(Table* table, int capacity, VirtualMachine* vm, Compiler* compiler) {
  Entry* entries = ALLOCATE(Entry, capacity, vm, compiler);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = EMPTY_VAL;
    entries[i].value = NAH_VAL;
  }
  
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (IS_EMPTY(entry->key)) continue;

    Entry* dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity, vm, compiler);
  table->entries = entries;
  table->capacity = capacity;
}

bool tableGet(Table* table, Value key, Value* value) {
  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (IS_EMPTY(entry->key)) return false;

  *value = entry->value;
  return true;
}

bool tableSet(Table* table, Value key, Value value, VirtualMachine* vm, Compiler* compiler) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity, vm, compiler);
  }

  Entry* entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = IS_EMPTY(entry->key);
  if (isNewKey && IS_NAH(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool tableDelete(Table* table, Value key, VirtualMachine* vm, Compiler* compiler) {
  if (table->count == 0) return false;

  // Find the entry
  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (IS_EMPTY(entry->key)) return false;

  // Place a tombstone in the entry.
  entry->key = EMPTY_VAL;
  entry->value = BOOL_VAL(true);
  return true;
}

void tableAddAll(Table* from, Table* to, VirtualMachine* vm, Compiler* compiler) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if (!IS_EMPTY(entry->key)) {
      tableSet(to, entry->key, entry->value, vm, compiler);
    }
  }
}

void printTable(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    printf("[ ");
    printValue(entry->key);
    printf(": ");
    printValue(entry->value);
    printf(" ]");
  }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash % table->capacity;

  for (;;) {
    Entry* entry = &table->entries[index];
    if (IS_EMPTY(entry->key)) {
      // Stop if we find an empty non-tombstone entry
      if (IS_NAH(entry->value)) return NULL;
    } else {
      ObjString* string = AS_STRING(entry->key);
      if (string->length == length && memcmp(string->chars, chars, length) == 0) {
        // We found it
        return string;
      }
    }
    index = (index + 1) % table->capacity;
  }
}

void markTable(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    markValue(entry->key);
    markValue(entry->value);
  } 
}

