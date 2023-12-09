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

  disassembleChunk(&chunk, "TEST CHUNK");
  interpret(&vm, &chunk);


  freeChunk(&chunk);
  freeVM(&vm);
  return 0;
}
