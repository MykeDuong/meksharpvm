#ifndef MEKVM_VALUE_H
#define MEKVM_VALUE_H

#include "common.h"

typedef double Value;

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void initValueArray(ValueArray *valueArray);
void writeValueArray(ValueArray *valueArray, Value value);
void freeValueArray(ValueArray *valueArray);

void printValue(Value value);

#endif /* MEKVM_VALUE_H */
