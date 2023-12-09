#include "bytechunk.h"
#include "common.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
  ByteChunk chunk;
  initChunk(&chunk);

  int constant = addConstant(&chunk, 1.2);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant, 123);

  writeChunk(&chunk, OP_RETURN, 123);
  disassembleChunk(&chunk, "TEST CHUNK");
  freeChunk(&chunk);
  return 0;
}
