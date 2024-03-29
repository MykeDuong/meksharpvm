#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bytechunk.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

VirtualMachine vm;

static void resetStack() { vm.stackTop = vm.stack; }

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);

  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm.ip - vm.byteChunk->code - 1;
  int line = vm.byteChunk->lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);
  resetStack();
}

void initVirtualMachine() {
  resetStack();
  initTable(&vm.strings);
  vm.objects = NULL;
}

void freeVirtualMachine() {
  freeTable(&vm.strings);
  freeObjects();
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool isFalsey(Value value) {
  return IS_NAH(value) || (IS_BOOLEAN(value) && !AS_BOOLEAN(value)) ||
         (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

static void concatenate() {
  ObjectString *b = AS_STRING(pop());
  ObjectString *a = AS_STRING(pop());
  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjectString *result = takeString(chars, length);
  push(CREATE_OBJECT_VALUE(result));
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.byteChunk->constants.values[READ_BYTE()])
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(valueType(a op b));                                                   \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(vm.byteChunk, (int)(vm.ip - vm.byteChunk->code));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NAH:
        push(CREATE_NAH_VALUE());
        break;
      case OP_TRUE:
        push(CREATE_BOOLEAN_VALUE(true));
        break;
      case OP_FALSE:
        push(CREATE_BOOLEAN_VALUE(false));
        break;
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(CREATE_BOOLEAN_VALUE(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:
        BINARY_OP(CREATE_BOOLEAN_VALUE, >);
        break;
      case OP_LESS:
        BINARY_OP(CREATE_BOOLEAN_VALUE, <);
        break;
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(CREATE_NUMBER_VALUE(a + b));
        } else {
          runtimeError("Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT:
        BINARY_OP(CREATE_NUMBER_VALUE, -);
        break;
      case OP_MULTIPLY:
        BINARY_OP(CREATE_NUMBER_VALUE, *);
        break;
      case OP_DIVIDE:
        BINARY_OP(CREATE_NUMBER_VALUE, /);
        break;
      case OP_NOT:
        push(CREATE_BOOLEAN_VALUE(isFalsey(pop())));
        break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(CREATE_NUMBER_VALUE(-AS_NUMBER(pop())));
        break;
      case OP_RETURN: {
        printValue(pop());
        printf("\n");
        return INTERPRET_OK;
      }
    }
  }
#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(const char *source) {
  ByteChunk byteChunk;
  initByteChunk(&byteChunk);

  if (!compile(source, &byteChunk)) {
    freeByteChunk(&byteChunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.byteChunk = &byteChunk;
  vm.ip = vm.byteChunk->code;

  InterpretResult result = run();
  freeByteChunk(&byteChunk);

  return result;
}
