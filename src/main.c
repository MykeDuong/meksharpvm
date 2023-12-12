#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

static void repl(VirtualMachine* vm) {
  char line[1024];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(vm, line);
  }
}

static char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*) malloc(fileSize + 1); // +1 for \0
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
  }

  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(VirtualMachine* vm, const char* path) {
  char* source = readFile(path);
  InterpretResult result = interpret(vm, source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
  VirtualMachine vm;
  initVM(&vm);

  if (argc == 1) {
    repl(&vm);
  } else if (argc == 2) {
    runFile(&vm, argv[1]);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
  }

  freeVM(&vm);
  return 0;
}
