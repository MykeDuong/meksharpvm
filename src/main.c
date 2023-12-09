#include "bytechunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[]) {
  ByteChunk chunk;
  initChunk(&chunk);

  VirtualMachine vm;
  initVM(&vm);

  writeConstant(&chunk, 12, 123);
  writeChunk(&chunk, OP_NEGATE, 123);
  writeChunk(&chunk, OP_RETURN, 124);
  interpret(&vm, &chunk);

  freeChunk(&chunk);
  freeVM(&vm);
  return 0;
}
