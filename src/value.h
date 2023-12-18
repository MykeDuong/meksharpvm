#ifndef MEKVM_VALUE_H
#define MEKVM_VALUE_H

#include "common.h"

typedef struct Object Object;
typedef struct ObjString ObjString;

typedef enum {
  VAL_BOOL,
  VAL_NAH,
  VAL_NUMBER,
  VAL_OBJECT,
  VAL_EMPTY,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Object* object;
  } as;
} Value;

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NAH(value)     ((value).type == VAL_NAH)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJECT(value)  ((value).type == VAL_OBJECT)
#define IS_EMPTY(value)   ((value).type == VAL_EMPTY)

#define AS_OBJECT(value)  ((value).as.object)
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

#define BOOL_VAL(value)   ((Value) { VAL_BOOL, {.boolean = value} })
#define NAH_VAL           ((Value) { VAL_NAH, {.number = 0}})
#define NUMBER_VAL(value) ((Value) { VAL_NUMBER, {.number = value} })
#define OBJECT_VAL(value) ((Value) { VAL_OBJECT, {.object = (Object*)value} })
#define EMPTY_VAL         ((Value) { VAL_EMPTY, {.number = 0} })
typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif 
