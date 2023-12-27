#ifndef MEKVM_OBJECT_H
#define MEKVM_OBJECT_H

#include "common.h"
#include "memory.h"
#include "value.h"
#include "bytechunk.h"

#define OBJECT_TYPE(value)         (AS_OBJECT(value)->type)

#define IS_FUNCTION(VALUE)         isObjectType(value, OBJ_FUNCTION)
#define IS_STRING(value)           isObjectType(value, OBJ_STRING)

#define AS_FUNCTION(value)         ((ObjFunction*)AS_OBJECT(value))
#define AS_STRING(value)           ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value)          (((ObjString*)AS_OBJECT(value))->chars)

typedef enum {
  OBJ_FUNCTION,
  OBJ_STRING,
} ObjectType;

struct Object {
  ObjectType type;
  struct Object* next;
};

typedef struct {
  Object object;
  int arity;
  ByteChunk chunk;
  ObjString* name;
} ObjFunction;

struct ObjString {
  Object object;
  bool isConstant;
  int length;
  uint32_t hash;
  const char* chars;
  char storage[];
};

ObjFunction* newFunction(VirtualMachine* vm);
ObjString* createString(VirtualMachine* vm, const char* chars, int length);
ObjString* createConstantString(VirtualMachine* vm, const char* chars, int length);
ObjString* completeString(VirtualMachine* vm, ObjString* string);
uint32_t hashValue(Value value);

void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif /* MEKVM_OBJECT_H */ 
