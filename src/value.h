#ifndef MEKVM_VALUE_H
#define MEKVM_VALUE_H

#include "common.h"

typedef enum {
  VALUE_BOOLEAN,
  VALUE_NAH,
  VALUE_NUMBER,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
  } as;
} Value;

#define IS_BOOLEAN(value) ((value).type == VALUE_BOOLEAN)
#define IS_NAH(value) ((value).type == VALUE_NAH)
#define IS_NUMBER(value) ((value).type == VALUE_NUMBER)

#define AS_BOOLEAN(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define CREATE_BOOLEAN_VALUE(value) ((Value){VALUE_BOOLEAN, {.boolean = value}})
#define CREATE_NAH_VALUE() ((Value){VALUE_NAH, {.number = 0}})
#define CREATE_NUMBER_VALUE(value) ((Value){VALUE_NUMBER, {.number = value}})

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);

void initValueArray(ValueArray *valueArray);
void writeValueArray(ValueArray *valueArray, Value value);
void freeValueArray(ValueArray *valueArray);

void printValue(Value value);

#endif /* MEKVM_VALUE_H */
