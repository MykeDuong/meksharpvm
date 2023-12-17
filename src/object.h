#ifndef MEKVM_OBJECT_H
#define MEKVM_OBJECT_H

#include "common.h"
#include "value.h"
#include "vm.h"

#define OBJECT_TYPE(value)         (AS_OBJECT(value)->type)

#define IS_STRING(value)           isObjectType(value, OBJ_STRING)

#define AS_STRING(value)           ((ObjString*)AS_OBJECT(value))
#define AS_CSTRING(value)          (((ObjString*)AS_OBJECT(value))->chars)

typedef enum {
  OBJ_STRING,
} ObjectType;

struct Object {
  ObjectType type;
};

struct ObjString {
  Object object;
  int length;
  char* chars;
};

ObjString* copyString(VirtualMachine* vm, const char* chars, int lenght);
void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif /* MEKVM_OBJECT_H */ 
