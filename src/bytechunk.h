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

void initChunk(ByteChunk* chunk, VirtualMachine* vm, Compiler* compiler);
void writeChunk(ByteChunk* chunk, uint8_t byte, int line, VirtualMachine* vm, Compiler* compiler);
int addConstant(ByteChunk* chunk, Value value, VirtualMachine* vm, Compiler* compiler);
void writeConstant(ByteChunk* chunk, Value value, int line, VirtualMachine* vm, Compiler* compiler);
void freeChunk(ByteChunk* chunk, VirtualMachine* vm, Compiler* compiler);

#endif 
