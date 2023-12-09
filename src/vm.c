#include "common.h"
#include "vm.h"

void initVM(VirtualMachine* vm) {
}


void freeVM(VirtualMachine* vm) {

}

InterpretResult interpret(VirtualMachine* vm, ByteChunk* chunk) {
  vm->chunk = chunk;
  vm->ip = vm->chunk->code;
  return run();
}

