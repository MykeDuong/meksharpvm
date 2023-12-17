#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJECT(vm, type, objectType) \
  (type*)allocateObject((vm), sizeof(type), objectType)

static Object* allocateObject(VirtualMachine* vm, size_t size, ObjectType type) {
  Object* obj = (Object*)reallocate(NULL, 0, size);
  obj->type = type;

  obj->next = vm->objects;
  vm->objects = obj;
  return obj;
}

ObjString* createString(VirtualMachine* vm, int length) {
  ObjString* string = (ObjString*)allocateObject(vm, sizeof(ObjString) + sizeof(char[length + 1]), OBJ_STRING);
  string->length = length;
  string->isConstant = false;
  string->chars = string->storage;
  return string;
}

ObjString* createConstantString(VirtualMachine* vm, const char* chars, int length) {
  ObjString* string = (ObjString*)allocateObject(vm, sizeof(ObjString), OBJ_STRING);
  string->length = length;
  string->isConstant = true;
  string->chars = chars;
  return string;  
}

void printObject(Value value) {
  switch (OBJECT_TYPE(value)) {
    case OBJ_STRING:
      printf("%.*s", AS_STRING(value)->length, AS_CSTRING(value));
      break;
    default: return; // Unreachable
  }
}
