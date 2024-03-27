#ifndef MEKVM_TABLE_H
#define MEKVM_TABLE_H

#include "common.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

typedef struct {
  ObjectString *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table);
bool tableGet(Table *table, ObjectString *key, Value *value);
bool tableSet(Table *table, ObjectString *key, Value value);
bool tableDelete(Table *table, ObjectString *key);
void tableAddAll(Table *from, Table *to);
ObjectString *tableFindString(Table *table, const char *chars, int length,
                              uint32_t hash);

#endif /* MEKVM_TABLE_H */
