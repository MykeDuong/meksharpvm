#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

void initValueArray(ValueArray *valueArray) {
  valueArray->values = NULL;
  valueArray->capacity = 0;
  valueArray->count = 0;
}

void writeValueArray(ValueArray *valueArray, Value value) {
  if (valueArray->capacity < valueArray->count + 1) {
    int oldCapacity = valueArray->capacity;
    valueArray->capacity = GROW_CAPACITY(oldCapacity);
    valueArray->values = GROW_ARRAY(Value, valueArray->values, oldCapacity,
                                    valueArray->capacity);
  }

  valueArray->values[valueArray->count] = value;
  valueArray->count++;
}

void freeValueArray(ValueArray *valueArray) {
  FREE_ARRAY(Value, valueArray->values, valueArray->capacity);
  initValueArray(valueArray);
}

void printValue(Value value) {
#ifdef NAN_BOXING
  if (IS_BOOLEAN(value)) {
    printf(AS_BOOLEAN(value) ? "true" : "false");
  } else if (IS_NAH(value)) {
    printf("nah");
  } else if (IS_NUMBER(value)) {
    printf("%g", AS_NUMBER(value));
  } else if (IS_OBJECT(value)) {
    printObject(value);
  } else {
    printf("Unidentified value");
  }
#else
  switch (value.type) {
    case VALUE_BOOLEAN:
      printf(AS_BOOLEAN(value) ? "true" : "false");
      break;
    case VALUE_NAH:
      printf("nah");
      break;
    case VALUE_NUMBER:
      printf("%g", AS_NUMBER(value));
      break;
    case VALUE_OBJECT:
      printObject(value);
      break;
  }
#endif
}

bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
  if (IS_NUMBER(a) && IS_NUMBER(b)) {
    return AS_NUMBER(a) == AS_NUMBER(b);
  }
  return a == b;
#else
  if (a.type != b.type)
    return false;
  switch (a.type) {
    case VALUE_BOOLEAN:
      return AS_BOOLEAN(a) == AS_BOOLEAN(b);
    case VALUE_NAH:
      return true;
    case VALUE_NUMBER:
      return AS_NUMBER(a) == AS_NUMBER(b);
    case VALUE_OBJECT: {
      return AS_OBJECT(a) == AS_OBJECT(b);
    }
    default:
      fprintf(stderr, "Unreachable code reached in valuesEqual()\n");
      return false;
  }
#endif
}
