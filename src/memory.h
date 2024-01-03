#ifndef MEKVM_MEMORY_H
#define MEKVM_MEMORY_H

#include "common.h"

typedef struct VirtualMachine_Struct VirtualMachine;

#define ALLOCATE(vm, type, count) \
  (type*)reallocate(vm, NULL, 0, sizeof(type) * (count))

#define FREE(vm, type, pointer) reallocate(vm, pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(vm, type, pointer, oldCount, newCount) \
  (type*) reallocate(vm, pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(vm, type, pointer, oldCount) \
  reallocate(vm, pointer, sizeof(type) * (oldCount), 0)

void* reallocate(VirtualMachine* vm, void* pointer, size_t oldSize, size_t newSize);
void collectGarbage(VirtualMachine* vm);
void freeObjects(VirtualMachine* vm);

#endif 

