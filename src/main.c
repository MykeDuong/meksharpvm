#include "bytechunk.h"
#include "common.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
  ByteChunk bytechunk;
  initByteChunk(&bytechunk);
  writeByte(&bytechunk, OP_RETURN);
    
  disassembleByteChunk(&bytechunk, "Test Bytechunk");

  freeByteChunk(&bytechunk);
  return 0;
}
