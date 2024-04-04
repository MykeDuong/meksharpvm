#ifndef MEKVM_OBJECT_H
#define MEKVM_OBJECT_H

#include "bytechunk.h"
#include "common.h"
#include "value.h"

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_CLOSURE(value) isObjectType(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) isObjectType(value, OBJECT_FUNCTION)
#define IS_NATIVE_FUNCTION(value) isObjectType(value, OBJECT_NATIVE_FUNCTION)
#define IS_STRING(value) isObjectType(value, OBJECT_STRING)

#define AS_CLOSURE(value) ((ObjectClosure *)AS_OBJECT(value))
#define AS_FUNCTION(value) ((ObjectFunction *)AS_OBJECT(value))
#define AS_NATIVE_FUNCTION(value)                                              \
  (((ObjectNativeFunction *)AS_OBJECT(value))->function)
#define AS_STRING(value) ((ObjectString *)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString *)AS_OBJECT(value))->chars)

typedef enum {
  OBJECT_FUNCTION,
  OBJECT_NATIVE_FUNCTION,
  OBJECT_STRING,
  OBJECT_CLOSURE,
  OBJECT_UPVALUE,
} ObjectType;

struct Object {
  ObjectType type;
  struct Object *next;
};

typedef struct {
  Object object;
  int arity; // Number of parameters
  int upvalueCount;
  ByteChunk byteChunk;
  ObjectString *name;
} ObjectFunction;

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct {
  Object object;
  NativeFn function;
} ObjectNativeFunction;

struct ObjectString {
  Object object;
  int length;
  char *chars;
  uint32_t hash;
};

typedef struct ObjectUpvalue {
  Object object;
  Value *location;
} ObjectUpvalue;

typedef struct {
  Object object;
  ObjectFunction *function;
  ObjectUpvalue **upvalues;
  int upvalueCount;
} ObjectClosure;

ObjectClosure *newClosure(ObjectFunction *function);
ObjectFunction *newFunction();
ObjectNativeFunction *newNativeFunction(NativeFn function);
ObjectString *takeString(char *chars, int length);
ObjectString *copyString(const char *chars, int length);
ObjectUpvalue *newUpvalue(Value *slot);
void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif /* MEKVM_OBJECT_H */
