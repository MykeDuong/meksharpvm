#ifndef MEKVM_COMPILER_H
#define MEKVM_COMPILER_H

#include "object.h"
#include "vm.h"

ObjectFunction *compile(const char *source);
void markCompilerRoots();

#endif /* MEKVM_COMPILER_H */
