#include <stdlib.h>

#include "bytechunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include "object.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

void* reallocate(VirtualMachine* vm, void* pointer, size_t oldSize, size_t newSize) {
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage(vm);
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

static void freeObject(VirtualMachine* vm, Object* obj) {
#ifdef DEBUG_LOG_GC 
  printf("%p free type %d\n", (void*)obj, obj->type);
#endif
  switch (obj->type) {
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*) obj;
      FREE_ARRAY(vm, ObjUpvalue*, closure->upvalues, closure->upvalueCount);
      FREE(vm, OBJ_CLOSURE, obj);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)obj;
      freeChunk(vm, &function->chunk);
      FREE(vm, ObjFunction, obj);
      break;
    }
    case OBJ_NATIVE: {
      FREE(vm, ObjNative, obj);
      break;
    }
    case OBJ_STRING: {
      ObjString* string = (ObjString*)obj;
      FREE(vm, ObjString, obj);
      break;
    }
    case OBJ_UPVALUE: {
      FREE(vm, ObjUpvalue, obj);
      break;
    }
  } 
}

static void markRoots() {

}

void collectGarbage(VirtualMachine* vm) {
#ifdef DEBUG_LOG_GC
  printf("---- Garbage Collector begins\n");
#endif

  markRoots();

#ifdef DEBUG_LOG_GC
  printf("---- Garbage Collector ends\n");
#endif
}

void freeObjects(VirtualMachine* vm) {
  Object* obj = vm->objects;
  while (obj != NULL) {
    Object* next = obj->next;
    freeObject(vm, obj);
    obj = next;
  }
}

