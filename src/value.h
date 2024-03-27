#ifndef MEKVM_VALUE_H
#define MEKVM_VALUE_H

#include "common.h"

typedef struct Object Object;
typedef struct ObjectString ObjectString;

typedef enum {
  VALUE_BOOLEAN,
  VALUE_NAH,
  VALUE_NUMBER,
  VALUE_OBJECT,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Object *object;
  } as;
} Value;

#define IS_BOOLEAN(value) ((value).type == VALUE_BOOLEAN)
#define IS_NAH(value) ((value).type == VALUE_NAH)
#define IS_NUMBER(value) ((value).type == VALUE_NUMBER)
#define IS_OBJECT(value) ((value).type == VALUE_OBJECT)

#define AS_BOOLEAN(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJECT(value) ((value).as.object)

#define CREATE_BOOLEAN_VALUE(value) ((Value){VALUE_BOOLEAN, {.boolean = value}})
#define CREATE_NAH_VALUE() ((Value){VALUE_NAH, {.number = 0}})
#define CREATE_NUMBER_VALUE(value) ((Value){VALUE_NUMBER, {.number = value}})
#define CREATE_OBJECT_VALUE(obj)                                               \
  ((Value){VALUE_OBJECT, {.object = (Object *)(obj)}})

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
