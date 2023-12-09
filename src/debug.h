#ifndef MEKVM_DEBUG_H
#define MEKVM_DEBUG_H

#include "bytechunk.h"

void disassembleChunk(ByteChunk* chunk, const char* name);
int disassembleInstruction(ByteChunk* chunk, int offset);

#endif 
