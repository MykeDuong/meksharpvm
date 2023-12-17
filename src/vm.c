#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "bytechunk.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "compiler.h"

static void resetStack(VirtualMachine* vm) {
  vm->stackTop = vm->stack;
}

static void runtimeError(VirtualMachine* vm, const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm->ip - vm->chunk->code - 1;
  int line = vm->chunk->lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);
  resetStack(vm);
}

void initVM(VirtualMachine* vm) {
  vm->stack = NULL;
  vm->stackCapacity = 0;
  resetStack(vm);
  vm->objects = NULL;
}

void freeVM(VirtualMachine* vm) {
  freeObjects(vm); 
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

static Value peek(VirtualMachine* vm, int distance) {
  return vm->stackTop[-1-distance];
}

static bool isFalsey(Value value) {
  return IS_NAH(value) || (IS_BOOL(value) && !AS_BOOL(value)) || (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

static void concatenate(VirtualMachine* vm) {
  ObjString* b = AS_STRING(pop(vm));
  ObjString* a = AS_STRING(pop(vm));

  int length = a->length + b->length;
  ObjString* result = createString(vm, length);
  memcpy(result->chars, a->chars, a->length);
  memcpy(result->chars + a->length, b->chars, b->length);
  result->chars[length] = '\0';

  push(vm, OBJECT_VAL(result));
}

static InterpretResult run(VirtualMachine* vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()]);
#define READ_CONSTANT_LONG() \
  ((vm->chunk->constants.values[READ_BYTE() | (READ_BYTE() << 8) | (READ_BYTE() << 16)]))

#define BINARY_OP(valueType, op) \
  do { \
    if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
      runtimeError(vm, "Operands must be numbers."); \
      return INTERPRET_RUNTIME_ERROR; \
    } \
    double b = AS_NUMBER(pop(vm)); \
    double a = AS_NUMBER(pop(vm)); \
    push(vm, valueType(a op b)); \
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
      case OP_NAH: push(vm, NAH_VAL); break;
      case OP_TRUE: push(vm, BOOL_VAL(true)); break;
      case OP_FALSE: push(vm, BOOL_VAL(false)); break;
      case OP_EQUAL: {
        Value b = pop(vm);
        Value a = pop(vm);
        push(vm, BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
      case OP_ADD: {
        if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
          concatenate(vm);
        } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
          double b = AS_NUMBER(pop(vm));
          double a = AS_NUMBER(pop(vm));
          push(vm, NUMBER_VAL(a + b));
        } else {
          runtimeError(vm, "Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT:
        push(vm, BOOL_VAL(isFalsey(pop(vm))));
        break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(vm, 0))) {
          runtimeError(vm, "Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
        break;
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

  if (!compile(vm, &chunk, source)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm->chunk = &chunk;
  vm->ip = vm->chunk->code;

  InterpretResult result = run(vm);

  freeChunk(&chunk);
  return INTERPRET_OK;
}

