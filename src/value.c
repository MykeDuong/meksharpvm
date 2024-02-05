#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "object.h"
#include "vm.h"
#include "compiler.h"

void initValueArray(ValueArray* array, VirtualMachine* vm, Compiler* compiler) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeValueArray(ValueArray* array, Value value, VirtualMachine* vm, Compiler* compiler) {
  if (array->capacity < array->count + 1) {
    // Grow
    int oldCap = array->capacity;
    array->capacity = GROW_CAPACITY(oldCap);
    array->values = GROW_ARRAY(Value, array->values, oldCap, array->capacity, vm, compiler);
  }

  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray* array, VirtualMachine* vm, Compiler* compiler) {
  FREE_ARRAY(Value, array->values, array->capacity, vm, compiler);
  initValueArray(array, vm, compiler);
}

void printValue(Value value) {
  switch (value.type) {
    case VAL_BOOL:
      printf(AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NAH: printf("nah"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break; 
    case VAL_OBJECT: printObject(value); break;
    case VAL_EMPTY: printf("EMPTY"); break;
  }
}

bool valuesEqual(Value a, Value b) {
  if (a.type != b.type) return false;
  switch (a.type) {
    case VAL_BOOL:      return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NAH:       return true;
    case VAL_NUMBER:    return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJECT:    return AS_OBJECT(a) == AS_OBJECT(b); 
    default:            return false; // Unreachable
  }
}
