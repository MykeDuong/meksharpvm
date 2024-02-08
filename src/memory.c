#include <stdlib.h>
#include <stdio.h>

#include "bytechunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include "object.h"
#include "compiler.h"
#include "value.h"

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

void markObject(Object* obj) {
  if (obj == NULL) return;

#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void*)obj);
  printValue(OBJECT_VAL(obj));
  printf("\n");
#endif 

  obj->isMarked = true;
}

void markValue(Value value) {
  if (IS_OBJECT(value)) markObject(AS_OBJECT(value));
}


static void freeObject(Object* obj, VirtualMachine* vm, Compiler* compiler) {
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)obj, obj->type);
#endif 
  switch (obj->type) {
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*) obj;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount, vm, compiler);
      FREE(OBJ_CLOSURE, obj, vm, compiler);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)obj;
      freeChunk(&function->chunk, vm, compiler);
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

static void markRoots(VirtualMachine* vm, Compiler* compiler) {
  if (vm != NULL) {
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
      markValue(*slot);
    }

    for (int i = 0; i < vm->frameCount; i++) {
      markObject((Object*)vm->frames[i].closure);
    }

    markTable(&vm->globals);
  }
}

void collectGarbage(VirtualMachine *vm, Compiler *compiler) {
#ifdef DEBUG_LOG_GC
  printf("---- Garbage Collection begins ----\n");
#endif
  
  //markRoots(vm, compiler);

#ifdef DEBUG_LOG_GC
  printf("---- Garbage Collection ends ----\n");
#endif 
}
