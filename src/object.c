#include <stdio.h>
#include <string.h>

#include "bytechunk.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJECT(type, objectType)                                      \
  (type *)allocateObject(sizeof(type), objectType)

static Object *allocateObject(size_t size, ObjectType type) {
  Object *object = (Object *)reallocate(NULL, 0, size);
  object->type = type;
  object->next = vm.objects;
  vm.objects = object;

#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif /* DEBUG_LOG_GC */

  return object;
}

ObjectClosure *newClosure(ObjectFunction *function) {
  ObjectUpvalue **upvalues = ALLOCATE(ObjectUpvalue *, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }
  ObjectClosure *closure = ALLOCATE_OBJECT(ObjectClosure, OBJECT_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

static ObjectString *allocateString(char *chars, int length, uint32_t hash) {
  ObjectString *string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  push(CREATE_OBJECT_VALUE(string));
  tableSet(&vm.strings, string, CREATE_NAH_VALUE());
  pop();

  return string;
}

static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjectClass *newClass(ObjectString *name) {
  ObjectClass *klass = ALLOCATE_OBJECT(ObjectClass, OBJECT_CLASS);
  klass->name = name;
  initTable(&klass->methods);
  return klass;
}

ObjectFunction *newFunction() {
  ObjectFunction *function = ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initByteChunk(&function->byteChunk);
  return function;
}

ObjectNativeFunction *newNativeFunction(NativeFn function) {
  ObjectNativeFunction *native =
      ALLOCATE_OBJECT(ObjectNativeFunction, OBJECT_NATIVE_FUNCTION);
  native->function = function;
  return native;
}

ObjectInstance *newInstance(ObjectClass *klass) {
  ObjectInstance *instance = ALLOCATE_OBJECT(ObjectInstance, OBJECT_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}

ObjectString *takeString(char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjectString *interned = tableFindString(&vm.strings, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash);
}

ObjectString *copyString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjectString *interned = tableFindString(&vm.strings, chars, length, hash);

  if (interned != NULL)
    return interned;

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

ObjectUpvalue *newUpvalue(Value *slot) {
  ObjectUpvalue *upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJECT_UPVALUE);
  upvalue->closed = CREATE_NAH_VALUE();
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

void printFunction(ObjectFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJECT_TYPE(value)) {
    case OBJECT_CLASS: {
      printf("%s", AS_CLASS(value)->name->chars);
      break;
    }
    case OBJECT_CLOSURE: {
      printFunction(AS_CLOSURE(value)->function);
      break;
    }
    case OBJECT_UPVALUE: {
      printf("upvalue");
      break;
    }
    case OBJECT_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;
    case OBJECT_NATIVE_FUNCTION:
      printf("<native fn>");
      break;
    case OBJECT_INSTANCE:
      printf("Instance of %s", AS_INSTANCE(value)->klass->name->chars);
      break;
    case OBJECT_STRING:
      printf("%s", AS_CSTRING(value));
      break;
  }
}
