#ifndef MEKVM_COMPILER_H
#define MEKVM_COMPILER_H

#include "bytechunk.h"
#include "scanner.h"
#include "common.h"

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

bool compile(const char* source, ByteChunk* chunk);

#endif 
