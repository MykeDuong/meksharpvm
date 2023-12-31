#ifndef MEKVM_CHUNK_H
#define MEKVM_CHUNK_H

#include "common.h"
#include "memory.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_NAH,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_CALL,
  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  OP_RETURN,
} OpCode;

typedef struct {
  // Bytecode
  int count;
  int capacity;
  uint8_t* code;

  // Line information with RLE: size + line
  int* lines;
  int lineSize;
  int lineCapacity;
  
  // Constants
  ValueArray constants;
} ByteChunk;

typedef struct VirtualMachine_Struct VirtualMachine;

void initChunk(ByteChunk* chunk);
void writeChunk(VirtualMachine* vm, ByteChunk* chunk, uint8_t byte, int line);
int addConstant(VirtualMachine* vm, ByteChunk* chunk, Value value);
void writeConstant(VirtualMachine* vm, ByteChunk* chunk, Value value, int line);
void freeChunk(VirtualMachine* vm, ByteChunk* chunk);

#endif 
