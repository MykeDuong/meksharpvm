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
  return obj;
}

static ObjString* allocateString(VirtualMachine* vm, char* chars, int length) {
  ObjString* string = ALLOCATE_OBJECT(vm, ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  return string;
}

ObjString* takeString(VirtualMachine* vm, char* chars, int length) {
  return allocateString(vm, chars, length);
}

ObjString* copyString(VirtualMachine* vm, const char* chars, int length) {
  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(vm, heapChars, length);
}

void printObject(Value value) {
  switch (OBJECT_TYPE(value)) {
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
    default: return; // Unreachable
  }
}
