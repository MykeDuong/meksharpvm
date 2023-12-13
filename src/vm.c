#include <stdio.h>

#include "bytechunk.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include "compiler.h"

static void resetStack(VirtualMachine* vm) {
  vm->stackTop = vm->stack;
}

void initVM(VirtualMachine* vm) {
  vm->stack = NULL;
  vm->stackCapacity = 0;
  resetStack(vm);
}

void freeVM(VirtualMachine* vm) {

}

void push(VirtualMachine* vm, Value value) {
  if (vm->stackCapacity < (int)(vm->stackTop - vm->stack) + 1) {
    // Grow the stack
    int oldCap = vm->stackCapacity;
    int pos = (int)(vm->stackTop - vm->stack);
    vm->stackCapacity = GROW_CAPACITY(oldCap);
    vm->stack = GROW_ARRAY(Value, vm->stack, oldCap, vm->stackCapacity);
    vm->stackTop = vm->stack + pos;
  }

  *vm->stackTop = value;
  vm->stackTop++;
}

Value pop(VirtualMachine* vm) {
  vm->stackTop--;
  return *vm->stackTop;
}


static InterpretResult run(VirtualMachine* vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()]);
#define READ_CONSTANT_LONG() \
  ((vm->chunk->constants.values[READ_BYTE() | (READ_BYTE() << 8) | (READ_BYTE() << 16)]))

#define BINARY_OP(op) \
  do { \
    double b = pop(vm); \
    double a = pop(vm); \
    push((vm), (a op b)); \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(vm, constant);
        break;
      }
      case OP_CONSTANT_LONG: {
        Value constant = READ_CONSTANT_LONG();
        push(vm, constant);
        break;
      }
      case OP_ADD:      BINARY_OP(+); break;
      case OP_SUBTRACT: BINARY_OP(-); break;
      case OP_MULTIPLY: BINARY_OP(*); break;
      case OP_DIVIDE:   BINARY_OP(/); break;
      case OP_NEGATE: push(vm, -pop(vm)); break;
      case OP_RETURN: {
        printValue(pop(vm));
        printf("\n");
        return INTERPRET_OK;
      }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT 
#undef READ_LONG_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(VirtualMachine* vm, const char* source) {
  ByteChunk chunk;
  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm->chunk = &chunk;
  vm->ip = vm->chunk->code;

  InterpretResult result = run(vm);

  freeChunk(&chunk);
  return INTERPRET_OK;
}

