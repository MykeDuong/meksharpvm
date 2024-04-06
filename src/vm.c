#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bytechunk.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

VirtualMachine vm;

static Value clockNative(int argCount, Value *args) {
  return CREATE_NUMBER_VALUE((double)clock() / CLOCKS_PER_SEC);
}

static Value printNative(int argCount, Value *args) {
  for (int i = 0; i < argCount; i++) {
    printValue(args[i]);
    if (i != argCount - 1)
      printf(" ");
  }
  printf("\n");
  return CREATE_NAH_VALUE();
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.openUpvalues = NULL;
  vm.frameCount = 0;
}

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);

  va_end(args);
  fputs("\n", stderr);

  fputs("Call stack:\n", stderr);
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjectFunction *function = frame->closure->function;
    // ip always points to the next instruction to execute
    size_t instruction = frame->ip - function->byteChunk.code - 1;
    fprintf(stderr, "[line %d] in ", function->byteChunk.lines[instruction]);
    if (function->name == NULL)
      fprintf(stderr, "script\n");
    else
      fprintf(stderr, "%s()\n", function->name->chars);
  }

  resetStack();
}

static void defineNativeFunction(const char *name, NativeFn function) {
  push(CREATE_OBJECT_VALUE(copyString(name, (int)strlen(name))));
  push(CREATE_OBJECT_VALUE(newNativeFunction(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVirtualMachine() {
  resetStack();
  vm.objects = NULL;
  vm.bytesAllocated = 0;
  vm.gcThreshold = 1024 * 1024;

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;

  initTable(&vm.globals);
  initTable(&vm.strings);

  defineNativeFunction("clock", clockNative);
  defineNativeFunction("printf", printNative);
}

void freeVirtualMachine() {
  freeTable(&vm.globals);
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

static bool call(ObjectClosure *closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d", closure->function->arity,
                 argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack Overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->byteChunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJECT(callee)) {
    switch (OBJECT_TYPE(callee)) {
      case OBJECT_CLASS: {
        ObjectClass *klass = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = CREATE_OBJECT_VALUE(newInstance(klass));
        return true;
      }
      case OBJECT_CLOSURE: {
        return call(AS_CLOSURE(callee), argCount);
      }
      case OBJECT_NATIVE_FUNCTION: {
        NativeFn native = AS_NATIVE_FUNCTION(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break; // Non-callable object called
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}

static ObjectUpvalue *captureUpvalue(Value *local) {
  // Traverse the list of open upvalues
  ObjectUpvalue *previousUpvalue = NULL;
  ObjectUpvalue *upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    // Not yet to the slot of the upvalue for the local
    previousUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  // There exist a slot in the open upvalues that captures the value
  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  // If there is no such captured upvalue, create a new one
  ObjectUpvalue *createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;

  if (previousUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    previousUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value *last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjectUpvalue *upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static void defineMethod(ObjectString *name) {
  Value method = peek(0);
  ObjectClass *klass = AS_CLASS(peek(1));
  tableSet(&klass->methods, name, method);
  pop();
}

static bool isFalsey(Value value) {
  return IS_NAH(value) || (IS_BOOLEAN(value) && !AS_BOOLEAN(value)) ||
         (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

static void concatenate() {
  ObjectString *b = AS_STRING(peek(0));
  ObjectString *a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjectString *result = takeString(chars, length);
  pop();
  pop();
  push(CREATE_OBJECT_VALUE(result));
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT()                                                        \
  (frame->closure->function->byteChunk.constants.values[READ_BYTE()])
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
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
    disassembleInstruction(
        &frame->closure->function->byteChunk,
        (int)(frame->ip - frame->closure->function->byteChunk.code));
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
      case OP_POP:
        pop();
        break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_SET_GLOBAL: {
        ObjectString *name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_GET_GLOBAL: {
        ObjectString *name = READ_STRING();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjectString *name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        // Push value referenced by the upvalue
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        // Deference Value slot referenced by the upvalue
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_GET_PROPERTY: {
        if (!IS_INSTANCE(peek(0))) {
          runtimeError("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjectInstance *instance = AS_INSTANCE(peek(0));
        ObjectString *name = READ_STRING();

        Value value;
        if (tableGet(&instance->fields, name, &value)) {
          pop(); // Instance
          push(value);
          break;
        }

        runtimeError("Undefined property '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      case OP_SET_PROPERTY: {
        // Stack: instance -> value
        // ByteChunk: [Instance -> Value Expression] -> OP_SET_PROPERTY -> field

        if (!IS_INSTANCE(peek(1))) {
          runtimeError("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjectInstance *instance = AS_INSTANCE(peek(1));
        tableSet(&instance->fields, READ_STRING(), peek(0));
        Value value = pop(); // Value
        pop();               // Instance
        push(value);         // Push value as the result of assignment
        break;
      }
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
      case OP_PRINT:
        printValue(pop());
        printf("\n");
        break;
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0)))
          frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        // callValue will update the frame array
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjectFunction *function = AS_FUNCTION(READ_CONSTANT());
        ObjectClosure *closure = newClosure(function);
        push(CREATE_OBJECT_VALUE(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] = captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE: {
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
      }
      case OP_RETURN: {
        Value result = pop();
        closeUpvalues(frame->slots);
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        break;
        return INTERPRET_OK;
      }
      case OP_CLASS: {
        push(CREATE_OBJECT_VALUE(newClass(READ_STRING())));
        break;
      }
      case OP_METHOD: {
        defineMethod(READ_STRING());
        break;
      }
    }
  }
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
}

InterpretResult interpret(const char *source) {
  ObjectFunction *function = compile(source);

  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  push(CREATE_OBJECT_VALUE(function));
  ObjectClosure *closure = newClosure(function);
  pop();
  push(CREATE_OBJECT_VALUE(closure));
  call(closure, 0);

  return run();
}
