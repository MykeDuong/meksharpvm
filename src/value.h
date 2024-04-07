#ifndef MEKVM_VALUE_H
#define MEKVM_VALUE_H

#include <string.h>

#include "common.h"

typedef struct Object Object;
typedef struct ObjectString ObjectString;

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)
#define TAG_NAH 1   // 01
#define TAG_FALSE 2 // 10
#define TAG_TRUE 3  // 11

typedef uint64_t Value;

#define IS_BOOLEAN(value) (((value) | 1) == CREATE_TRUE_VALUE())
#define IS_NAH(value) ((value) == CREATE_NAH_VALUE())
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_OBJECT(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOLEAN(value) ((value) == CREATE_TRUE_VALUE())
#define AS_NUMBER(value) valueToNum(value)
#define AS_OBJECT(value) ((Object *)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))
// SIGNBIT | QNAN = 11111...11000..0

#define CREATE_BOOLEAN_VALUE(value)                                            \
  ((value) ? CREATE_TRUE_VALUE() : CREATE_FALSE_VALUE())
#define CREATE_FALSE_VALUE() ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define CREATE_TRUE_VALUE() ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define CREATE_NAH_VALUE() ((Value)(uint64_t)(QNAN | TAG_NAH))
#define CREATE_NUMBER_VALUE(num) numToValue(num)
#define CREATE_OBJECT_VALUE(object)                                            \
  (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(object))

static inline double valueToNum(Value value) {
  double num;
  memcpy(&num, &value, sizeof(Value));
  return num;
}

static inline Value numToValue(double num) {
  Value value;
  memcpy(&value, &num, sizeof(double));
  return value;
}

#else

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

#endif /* NAN_BOXING */

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
