#ifndef MEKVM_TABLE_H
#define MEKVM_TABLE_H

#include "common.h"
#include "memory.h"
#include "value.h"

typedef struct {
  Value key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(VirtualMachine* vm, Table* table);
bool tableGet(VirtualMachine* vm, Table* table, Value key, Value* value);
bool tableSet(VirtualMachine* vm, Table* table, Value key, Value value);
bool tableDelete(Table* table, Value key);
void tableAddAll(VirtualMachine* vm, Table* from, Table* to);
void printTable(Table* table);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

#endif /* MEKVM_TABLE_H */ 
