#include <stdlib.h>

#include "bytechunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include "object.h"
#include "compiler.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize, VirtualMachine* vm, Compiler* compiler) {
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage(vm, compiler);
#endif
  }
  if (newSize == 0) {
    // free memory
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}

static void freeObject(Object* obj, VirtualMachine* vm, Compiler* compiler) {
  switch (obj->type) {
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*) obj;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount, vm, compiler);
      FREE(OBJ_CLOSURE, obj, vm, compiler);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)obj;
      freeChunk(&function->chunk);
      FREE(ObjFunction, obj, vm, compiler);
      break;
    }
    case OBJ_NATIVE: {
      FREE(ObjNative, obj, vm, compiler);
      break;
    }
    case OBJ_STRING: {
      ObjString* string = (ObjString*)obj;
      FREE(ObjString, obj, vm, compiler);
      break;
    }
    case OBJ_UPVALUE: {
      FREE(ObjUpvalue, obj, vm, compiler);
      break;
    }
  } 
}

void freeObjects(VirtualMachine* vm, Compiler* compiler) {
  Object* obj = vm->objects;
  while (obj != NULL) {
    Object* next = obj->next;
    freeObject(obj, vm, compiler);
    obj = next;
  }
}

