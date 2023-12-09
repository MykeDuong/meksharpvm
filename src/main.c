#include "bytechunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[]) {
  ByteChunk chunk;
  initChunk(&chunk);

  VirtualMachine vm;
  initVM(&vm);
  
  for (int i = 0; i < 512; i++) {
    writeConstant(&chunk, 69420, i);
  }

  for (int i = 0; i < 12; i++) {
    writeConstant(&chunk, 42069, i + 512);
  }

  writeChunk(&chunk, OP_RETURN, 13);
  interpret(&vm, &chunk);
  //disassembleChunk(&chunk, "TEST CHUNK");

  freeChunk(&chunk);
  freeVM(&vm);
  return 0;
}
