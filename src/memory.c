#include <stdlib.h>

#include "bytechunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include "object.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    // free memory
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}

static void freeObject(Object* obj) {
  switch (obj->type) {
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*) obj;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
      FREE(OBJ_CLOSURE, obj);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)obj;
      freeChunk(&function->chunk);
      FREE(ObjFunction, obj);
      break;
    }
    case OBJ_NATIVE: {
      FREE(ObjNative, obj);
      break;
    }
    case OBJ_STRING: {
      ObjString* string = (ObjString*)obj;
      FREE(ObjString, obj);
      break;
    }
    case OBJ_UPVALUE: {
      FREE(ObjUpvalue, obj);
      break;
    }
  } 
}

void freeObjects(VirtualMachine* vm) {
  Object* obj = vm->objects;
  while (obj != NULL) {
    Object* next = obj->next;
    freeObject(obj);
    obj = next;
  }
}

