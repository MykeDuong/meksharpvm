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

void initTable(Table* table, VirtualMachine* vm, Compiler* compiler);
void freeTable(Table* table, VirtualMachine* vm, Compiler* compiler);
bool tableGet(Table* table, Value key, Value* value);
bool tableSet(Table* table, Value key, Value value, VirtualMachine* vm, Compiler* compiler);
bool tableDelete(Table* table, Value key, VirtualMachine* vm, Compiler* compiler);
void tableAddAll(Table* from, Table* to, VirtualMachine* vm, Compiler* compiler);
void printTable(Table* table);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

#endif /* MEKVM_TABLE_H */ 
