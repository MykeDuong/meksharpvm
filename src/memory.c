#include <stdlib.h>

#include "bytechunk.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include <stdio.h>
#endif /* DEBUG_LOG_GC */

#define GC_HEAP_GROWTH_FACTOR 2

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += (newSize - oldSize);
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif /* DEBUG_STRESS_GC */
  }

  if (vm.bytesAllocated > vm.gcThreshold) {
    collectGarbage();
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);

  if (result == NULL)
    exit(1);

  return result;
}

void markObject(Object *object) {
  if (object == NULL || object->isMarked)
    return;

#ifdef DEBUG_LOG_GC
  // Log the object being marked
  printf("%p mark ", (void *)object);
  printValue(CREATE_OBJECT_VALUE(object));
  printf("\n");
#endif /* DEBUG_LOG_GC */

  object->isMarked = true;

  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    vm.grayStack =
        (Object **)realloc(vm.grayStack, sizeof(Object *) * vm.grayCapacity);

    if (vm.grayStack == NULL)
      exit(1);
  }

  vm.grayStack[vm.grayCount++] = object;
}

static void freeObject(Object *object) {
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void *)object, object->type);
#endif
  switch (object->type) {
    case OBJECT_CLOSURE: {
      ObjectClosure *closure = (ObjectClosure *)object;
      FREE_ARRAY(ObjectUpvalue *, closure->upvalues, closure->upvalueCount);
      FREE(ObjectClosure, object);
      break;
    }
    case OBJECT_UPVALUE: {
      FREE(ObjectUpvalue, object);
      break;
    }
    case OBJECT_FUNCTION: {
      ObjectFunction *function = (ObjectFunction *)object;
      freeByteChunk(&function->byteChunk);
      FREE(ObjectFunction, object);
      break;
    }
    case OBJECT_NATIVE_FUNCTION: {
      FREE(OBJECT_NATIVE_FUNCTION, object);
      break;
    }
    case OBJECT_STRING: {
      ObjectString *string = (ObjectString *)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjectString, object);
      break;
    }
  }
}

void markValue(Value value) {
  if (IS_OBJECT(value))
    markObject(AS_OBJECT(value));
}

static void markArray(ValueArray *array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

static void blackenObject(Object *object) {
#ifdef DEBUG_LOG_GC

  printf("%p blacken ", (void *)object);
  printValue(CREATE_OBJECT_VALUE(object));
  printf("\n");

#endif /* DEBUG_LOG_GC */
  switch (object->type) {
    case OBJECT_CLOSURE: {
      ObjectClosure *closure = (ObjectClosure *)object;
      markObject((Object *)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) {
        markObject((Object *)closure->upvalues[i]);
      }
      break;
    }
    case OBJECT_FUNCTION: {
      ObjectFunction *function = (ObjectFunction *)object;
      markObject((Object *)function->name);
      markArray(&function->byteChunk.constants);
      break;
    }
    case OBJECT_UPVALUE:
      markValue(((ObjectUpvalue *)object)->closed);
      break;
    case OBJECT_NATIVE_FUNCTION:
    case OBJECT_STRING:
      break;
  }
}

static void markRoots() {
  for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  markTable(&vm.globals);

  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Object *)vm.frames[i].closure);
  }

  for (ObjectUpvalue *upvalue = vm.openUpvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    markObject((Object *)upvalue);
  }

  markCompilerRoots();
}

static void traceReferences() {
  while (vm.grayCount > 0) {
    Object *object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}

static void sweep() {
  Object *previous = NULL;
  Object *object = vm.objects;
  while (object != NULL) {
    if (object->isMarked) {
      // Unmark and continue on
      object->isMarked = false;
      previous = object;
      object = object->next;
    } else {
      Object *unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }

      freeObject(unreached);
    }
  }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
  printf("---- Begin Garbage Collection ----\n");
  size_t before = vm.bytesAllocated;
#endif /* DEBUG_LOG_GC */

  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings);
  sweep();

  vm.gcThreshold = vm.bytesAllocated * GC_HEAP_GROWTH_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("---- Result: Collected %zu bytes (from %zu to %zu) next threshold at "
         "%zu\n",
         before - vm.bytesAllocated, before, vm.bytesAllocated, vm.gcThreshold);
  printf("---- End Garbage Collection ----\n");
#endif /* DEBUG_LOG_GC */
}

void freeObjects() {
  Object *object = vm.objects;
  while (object != NULL) {
    Object *next = object->next;
    freeObject(object);
    object = next;
  }

  free(vm.grayStack);
}
