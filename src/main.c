#include "bytechunk.h"
#include "common.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
  ByteChunk chunk;
  initChunk(&chunk);

  for (int i = 0; i < 512; i++) {
    writeConstant(&chunk, 69420, i);
  }  

  disassembleChunk(&chunk, "TEST CHUNK");
  freeChunk(&chunk);
  return 0;
}
