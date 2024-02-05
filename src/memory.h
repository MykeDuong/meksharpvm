#ifndef MEKVM_MEMORY_H
#define MEKVM_MEMORY_H

#include "common.h"

typedef struct VirtualMachine_Struct VirtualMachine;
typedef struct Compiler_Struct Compiler;

#define ALLOCATE(type, count, vm, compiler) \
  (type*)reallocate(NULL, 0, sizeof(type) * (count), vm, compiler)

#define FREE(type, pointer, vm, compiler) reallocate(pointer, sizeof(type), 0, vm, compiler)

#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount, vm, compiler) \
  (type*) reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount), vm, compiler)

#define FREE_ARRAY(type, pointer, oldCount, vm, compiler) \
  reallocate(pointer, sizeof(type) * (oldCount), 0, vm, compiler)

void* reallocate(void* pointer, size_t oldSize, size_t newSize, VirtualMachine* vm, Compiler* compiler);
void collectGarbage(VirtualMachine* vm, Compiler* compiler);
void freeObjects(VirtualMachine* vm, Compiler* compiler);

#endif 

