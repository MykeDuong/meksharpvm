#include "bytechunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
  initVirtualMachine();
  ByteChunk byteChunk;
  initByteChunk(&byteChunk);

  int constant = addConstant(&byteChunk, 1.2);
  writeByteChunk(&byteChunk, OP_CONSTANT, 123);
  writeByteChunk(&byteChunk, constant, 123);

  constant = addConstant(&byteChunk, 3.4);
  writeByteChunk(&byteChunk, OP_CONSTANT, 123);
  writeByteChunk(&byteChunk, constant, 123);

  writeByteChunk(&byteChunk, OP_ADD, 123);

  constant = addConstant(&byteChunk, 5.6);
  writeByteChunk(&byteChunk, OP_CONSTANT, 123);
  writeByteChunk(&byteChunk, constant, 123);

  writeByteChunk(&byteChunk, OP_DIVIDE, 123);

  writeByteChunk(&byteChunk, OP_NEGATE, 123);
  writeByteChunk(&byteChunk, OP_RETURN, 123);

  disassembleByteChunk(&byteChunk, "Test Chunk");
  interpret(&byteChunk);
  freeVirtualMachine();
  freeByteChunk(&byteChunk);
  return 0;
}
