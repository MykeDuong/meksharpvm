#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define HASH_SEED 2166136261u
#define HASH_PRIME 16777619

#define ALLOCATE_OBJECT(vm, type, objectType) \
  (type*)allocateObject((vm), sizeof(type), objectType)

static Object* allocateObject(VirtualMachine* vm, size_t size, ObjectType type) {
  Object* obj = (Object*)reallocate(NULL, 0, size);
  obj->type = type;

  obj->next = vm->objects;
  vm->objects = obj;
  return obj;
}

static uint32_t hashString(const char* key, int length) {
  uint32_t hash = HASH_SEED;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= HASH_PRIME;
  }
  return hash;
}

static uint32_t hashNumber(double value) {
  uint32_t hash = HASH_SEED;
  hash ^= (uint32_t)value;
  hash *= HASH_PRIME;
  return hash;
}

uint32_t hashValue(Value value) {
  switch (value.type) {
    case VAL_BOOL:      return AS_BOOL(value) ? 3: 5;
    case VAL_NAH:       return 7;
    case VAL_NUMBER:    return hashNumber(AS_NUMBER(value));
    case VAL_OBJECT:    return AS_STRING(value)->hash;
    case VAL_EMPTY:     return 0;
  }
}

ObjString* createString(VirtualMachine* vm, const char* chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
  if (interned != NULL) return interned;
    
  ObjString* string = (ObjString*)allocateObject(vm, sizeof(ObjString) + sizeof(char[length + 1]), OBJ_STRING);
  string->length = length;
  string->isConstant = false;
  memcpy(string->storage, chars, length);
  string->storage[length] = '\0';
  string->chars = string->storage;
  string->hash = hash;
  tableSet(&vm->strings, OBJECT_VAL(string), NAH_VAL);
  return string;
}

ObjString* createConstantString(VirtualMachine* vm, const char* chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
  if (interned != NULL) return interned;

  ObjString* string = (ObjString*)allocateObject(vm, sizeof(ObjString), OBJ_STRING);
  string->length = length;
  string->isConstant = true;
  string->chars = chars;
  string->hash = hash;
  tableSet(&vm->strings, OBJECT_VAL(string), NAH_VAL);
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
