#ifndef MVM_DEBUG_H
#define MVM_DEBUG_H

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);


#endif 
