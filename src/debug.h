#ifndef MEKVM_DEBUG_H
#define MEKVM_DEBUG_H

#include "bytechunk.h"

void disassembleByteChunk(ByteChunk *bytechunk, const char *name);
int disassembleInstruction(ByteChunk *bytechunk, int offset);

#endif /* MEKVM_DEBUG_H */
