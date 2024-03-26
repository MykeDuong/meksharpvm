#include "bytechunk.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
  ByteChunk byteChunk;
  initByteChunk(&byteChunk);

  int constant = addConstant(&byteChunk, 1.2);
  writeByteChunk(&byteChunk, OP_CONSTANT, 123);
  writeByteChunk(&byteChunk, constant, 123);
  writeByteChunk(&byteChunk, OP_RETURN, 123);

  disassembleByteChunk(&byteChunk, "Test Chunk");
  freeByteChunk(&byteChunk);
  return 0;
}
