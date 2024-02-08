#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "bytechunk.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include "compiler.h"

static Value clockNative(int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack(VirtualMachine* vm) {
  vm->stackTop = vm->stack;
  vm->frameCount = 0;
  vm->openUpvalues = NULL;
}

static void runtimeError(VirtualMachine* vm, const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm->frameCount - 1; i >= 0; i--) {
    CallFrame* frame=  &vm->frames[i];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", getLine(&function->chunk, instruction)); 
    
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%.*s()\n", function->name->length, function->name->chars);
    }
  }

  resetStack(vm);
}

static void defineNative(VirtualMachine* vm, const char* name, NativeFn function) {
  push(vm, OBJECT_VAL(createString(vm, NULL, name, (int)strlen(name))));
  push(vm, OBJECT_VAL(newNative(vm, NULL, function)));
  tableSet(&vm->globals, vm->stack[0], vm->stack[1], vm, NULL);
  pop(vm);
  pop(vm);
}

void initVM(VirtualMachine* vm) {
  vm->stack = ALLOCATE(Value, UINT8_COUNT, vm, NULL);
  vm->stackCapacity = UINT8_COUNT;
  resetStack(vm);
  vm->objects = NULL;
  initTable(&vm->strings, vm, NULL);
  initTable(&vm->globals, vm, NULL);
  defineNative(vm, "clock", clockNative);
}

void freeVM(VirtualMachine* vm) {
  freeTable(&vm->strings, vm, NULL);
  freeTable(&vm->globals, vm, NULL);
  freeTable(&vm->globals, vm, NULL);
  freeObjects(vm, NULL);
  FREE_ARRAY(Value, vm->stack, vm->stackCapacity, vm, NULL);
}

void push(VirtualMachine* vm, Value value) {
  if (vm->stackCapacity < (int)(vm->stackTop - vm->stack) + 1) {
    // Grow the stack
    int oldCap = vm->stackCapacity;
    int pos = (int)(vm->stackTop - vm->stack);
    vm->stackCapacity = GROW_CAPACITY(oldCap);
    vm->stack = GROW_ARRAY(Value, vm->stack, oldCap, vm->stackCapacity, vm, NULL);
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

static bool call(VirtualMachine* vm, ObjClosure* closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError(vm, "Expected %d arguments, but got %d.", closure->function->arity, argCount);
    return false;
  }

  if (vm->frameCount == FRAMES_MAX) {
    runtimeError(vm, "Stack Overflow.");
    return false;
  }

  CallFrame* frame = &vm->frames[vm->frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm->stackTop - vm->stack - argCount - 1;
  return true;
}

static bool callValue(VirtualMachine* vm, Value callee, int argCount) {
  if (IS_OBJECT(callee)) {
    switch (OBJECT_TYPE(callee)) {
      case OBJ_CLOSURE:
        return call(vm, AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm->stackTop - argCount);
        vm->stackTop -= argCount + 1;
        push(vm, result);
        return true;
      }
      default:
        break;
    }
  }
  runtimeError(vm, "Can only call functions and classes.");
  return false;
}

static ObjUpvalue* captureUpvalue(VirtualMachine* vm, Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm->openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue* createdUpvalue = newUpvalue(vm, NULL, local);

  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm->openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(VirtualMachine* vm, Value* last) {
  while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm->openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm->openUpvalues = upvalue->next;
  }
}

static bool isFalsey(Value value) {
  return IS_NAH(value) || (IS_BOOL(value) && !AS_BOOL(value)) || (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

static void concatenate(VirtualMachine* vm) {
  ObjString* b = AS_STRING(pop(vm));
  ObjString* a = AS_STRING(pop(vm));

  int length = a->length + b->length;
  char chars[length + 1];
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';
  ObjString* result = createString(vm, NULL, chars, length);

  push(vm, OBJECT_VAL(result));
}

static InterpretResult run(VirtualMachine* vm) {
  CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() \
  ((frame->closure->function->chunk.constants.values[READ_BYTE() | (READ_BYTE() << 8) | (READ_BYTE() << 16)]))
#define READ_SHORT() \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())

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
    disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
      case OP_POP: pop(vm); break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(vm, vm->stack[frame->slots  + slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        vm->stack[frame->slots  + slot] = peek(vm, 0);
        break;
      }
      case OP_GET_GLOBAL: {
        Value name = READ_CONSTANT();
        ObjString* nameString = AS_STRING(name);
        Value value;
        if (!tableGet(&vm->globals, name, &value)) {
          runtimeError(vm, "Undefined variable '%.*s'.", nameString->length, nameString->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(vm, value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        Value name = READ_CONSTANT(); 
        tableSet(&vm->globals, name, peek(vm, 0), vm, NULL);
        pop(vm);
        break;
      }
      case OP_SET_GLOBAL: {
        Value name = READ_CONSTANT();
        if (tableSet(&vm->globals, name, peek(vm, 0), vm, NULL)) {
          tableDelete(&vm->globals, name, vm, NULL);
          ObjString* nameString = AS_STRING(name);
          runtimeError(vm, "Undefined variable '%.*s'", nameString->length, nameString->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(vm, *frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(vm, 0);
        break;
      }
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
      case OP_PRINT: {
        printValue(pop(vm));
        printf("\n");
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(vm, 0))) frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(vm, peek(vm, argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm->frames[vm->frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure* closure = newClosure(vm, NULL, function);
        push(vm, OBJECT_VAL(closure));

        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] = captureUpvalue(vm, &vm->stack[frame->slots + index]);
          } else {
            printf("%d\n", frame->closure->upvalueCount);
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }

        break;
      }
      case OP_CLOSE_UPVALUE: {
        closeUpvalues(vm, vm->stackTop - 1);
        pop(vm);
        break;
      }
      case OP_RETURN: {
        Value result = pop(vm);
        closeUpvalues(vm, &vm->stack[frame->slots]);
        vm->frameCount--;
        if (vm->frameCount == 0) {
          pop(vm);
          return INTERPRET_OK;
        }

        vm->stackTop = &vm->stack[frame->slots];
        push(vm, result);
        frame = &vm->frames[vm->frameCount - 1];
        break;
      }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT 
#undef READ_LONG_CONSTANT
#undef READ_SHORT
#undef BINARY_OP
}

InterpretResult interpret(VirtualMachine* vm, const char* source) {
  ObjFunction* function = compile(vm, source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(vm, OBJECT_VAL(function));
  ObjClosure* closure = newClosure(vm, NULL, function);
  pop(vm);
  push(vm, OBJECT_VAL(closure));
  call(vm, closure, 0);

  return run(vm);

}

