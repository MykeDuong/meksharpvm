#ifndef MEKVM_OBJECT_H
#define MEKVM_OBJECT_H

#include "common.h"
#include "memory.h"
#include "value.h"
#include "bytechunk.h"

#define OBJECT_TYPE(value)         (AS_OBJECT(value)->type)

#define IS_CLOSURE(value)          isObjectType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)         isObjectType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)           isObjectType(value, OBJ_NATIVE)
#define IS_STRING(value)           isObjectType(value, OBJ_STRING)

#define AS_CLOSURE(value)          ((ObjClosure*)AS_OBJECT(value))
#define AS_FUNCTION(value)         ((ObjFunction*)AS_OBJECT(value))
#define AS_NATIVE(value)           (((ObjNative*)AS_OBJECT(value))->function)
#define AS_STRING(value)           ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value)          (((ObjString*)AS_OBJECT(value))->chars)

typedef enum {
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE,
} ObjectType;

struct Object {
  ObjectType type;
  struct Object* next;
};

typedef struct {
  Object object;
  int arity;
  int upvalueCount;
  ByteChunk chunk;
  ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Object object;
  NativeFn function;
} ObjNative;

struct ObjString {
  Object object;
  bool isConstant;
  int length;
  uint32_t hash;
  const char* chars;
  char storage[];
};

typedef struct ObjUpvalue {
  Object object;
  Value* location;
  Value closed;
  struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
  Object object;
  ObjFunction* function;
  ObjUpvalue** upvalues;
  int upvalueCount;
} ObjClosure;


ObjClosure* newClosure(VirtualMachine* vm, ObjFunction* function);
ObjFunction* newFunction(VirtualMachine* vm);
ObjNative* newNative(VirtualMachine* vm, NativeFn function);
ObjString* createString(VirtualMachine* vm, const char* chars, int length);
ObjString* createConstantString(VirtualMachine* vm, const char* chars, int length);
ObjString* completeString(VirtualMachine* vm, ObjString* string);
ObjUpvalue* newUpvalue(VirtualMachine* vm, Value* slot);
void printObject(Value value);
uint32_t hashValue(Value value);

static inline bool isObjectType(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif /* MEKVM_OBJECT_H */ 
