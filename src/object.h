#ifndef MEKVM_OBJECT_H
#define MEKVM_OBJECT_H

#include "bytechunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_BOUND_METHOD(value) isObjectType(value, OBJECT_BOUND_METHOD)
#define IS_CLASS(value) isObjectType(value, OBJECT_CLASS)
#define IS_CLOSURE(value) isObjectType(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) isObjectType(value, OBJECT_FUNCTION)
#define IS_NATIVE_FUNCTION(value) isObjectType(value, OBJECT_NATIVE_FUNCTION)
#define IS_STRING(value) isObjectType(value, OBJECT_STRING)
#define IS_INSTANCE(value) isObjectType(value, OBJECT_INSTANCE)

#define AS_BOUND_METHOD(value) ((ObjectBoundMethod *)AS_OBJECT(value))
#define AS_CLASS(value) ((ObjectClass *)AS_OBJECT(value))
#define AS_CLOSURE(value) ((ObjectClosure *)AS_OBJECT(value))
#define AS_FUNCTION(value) ((ObjectFunction *)AS_OBJECT(value))
#define AS_INSTANCE(value) ((ObjectInstance *)AS_OBJECT(value))
#define AS_NATIVE_FUNCTION(value)                                              \
  (((ObjectNativeFunction *)AS_OBJECT(value))->function)
#define AS_STRING(value) ((ObjectString *)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString *)AS_OBJECT(value))->chars)

typedef enum {
  OBJECT_BOUND_METHOD,
  OBJECT_CLASS,
  OBJECT_FUNCTION,
  OBJECT_NATIVE_FUNCTION,
  OBJECT_STRING,
  OBJECT_CLOSURE,
  OBJECT_UPVALUE,
  OBJECT_INSTANCE,
} ObjectType;

struct Object {
  ObjectType type;
  bool isMarked;
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
  Value closed;
  struct ObjectUpvalue *next;
} ObjectUpvalue;

typedef struct {
  Object object;
  ObjectFunction *function;
  ObjectUpvalue **upvalues;
  int upvalueCount;
} ObjectClosure;

typedef struct {
  Object object;
  ObjectString *name;
  Table methods;
} ObjectClass;

typedef struct {
  Object object;
  ObjectClass *klass;
  Table fields;
} ObjectInstance;

typedef struct {
  Object object;
  Value receiver;
  ObjectClosure *method;
} ObjectBoundMethod;

ObjectBoundMethod *newBoundMethod(Value receiver, ObjectClosure *method);
ObjectClass *newClass(ObjectString *name);
ObjectClosure *newClosure(ObjectFunction *function);
ObjectFunction *newFunction();
ObjectInstance *newInstance(ObjectClass *klass);
ObjectNativeFunction *newNativeFunction(NativeFn function);
ObjectString *takeString(char *chars, int length);
ObjectString *copyString(const char *chars, int length);
ObjectUpvalue *newUpvalue(Value *slot);
void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif /* MEKVM_OBJECT_H */
