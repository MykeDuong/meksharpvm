#ifndef MEKVM_COMPILER_H
#define MEKVM_COMPILER_H

#include "scanner.h"
#include "common.h"
#include "vm.h"
#include "object.h"

ObjFunction* compile(VirtualMachine* vm, const char* source);

#endif 
