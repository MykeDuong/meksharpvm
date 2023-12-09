#ifndef MEKVM_VM_H
#define MEKVM_VM_H

#include "bytechunk.h"
#include <stdint.h>

typedef struct {
  ByteChunk* chunk;
  uint8_t* ip;
} VirtualMachine;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;


void initVM(VirtualMachine* vm);
void freeVM(VirtualMachine* vm);
InterpretResult interpret(VirtualMachine* vm, ByteChunk* chunk);



#endif 
