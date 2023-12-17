#ifndef MEKVM_COMPILER_H
#define MEKVM_COMPILER_H

#include "bytechunk.h"
#include "scanner.h"
#include "common.h"
#include "vm.h"
#include "object.h"

bool compile(VirtualMachine* vm, ByteChunk* chunk, const char* source);

#endif 
