#include <stdlib.h>

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
    case OBJ_STRING: {
      ObjString* string = (ObjString*)obj;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, obj);
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

