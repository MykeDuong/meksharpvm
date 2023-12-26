#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytechunk.h"
#include "compiler.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif /* ifdef DEBUG_PRINT_CODE */ 

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef struct {
  Token name;
  int depth;
} Local;

typedef struct {
  Local locals[UINT8_COUNT];
  int localCount;
  int scopeDepth;
} Compiler;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and 
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >= 
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . () 
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(
  VirtualMachine* vm,
  Compiler* currentCompiler,
  Parser* parser, 
  Scanner* scanner,
  ByteChunk* compilingChunk,
  bool canAssign
);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;



static ByteChunk* currentChunk(ByteChunk* compilingChunk) {
  return compilingChunk;
}

static void errorAt(Parser* parser, Token* token, const char* message) {
  if (parser->panicMode) return;
  parser->panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser->hadError = true;
}

static void error(Parser* parser, const char* message) {
  errorAt(parser, &parser->previous, message);
}

static void errorAtCurrent(Parser* parser, const char* message) {
  errorAt(parser, &parser->current, message);
}

static void advance(Parser* parser, Scanner* scanner) {
  parser->previous = parser->current;
  for (;;) {
    parser->current = scanToken(scanner);
    if (parser->current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser, parser->current.start);
  }
}

static void consume(Parser* parser, Scanner* scanner, TokenType type, const char* message) {
  if (parser->current.type == type) {
    advance(parser, scanner);
    return;
  }

  errorAtCurrent(parser, message);
}

static bool check(Parser* parser, TokenType type) {
  return parser->current.type == type;
}

static bool match(Parser* parser, Scanner* scanner, TokenType type) {
  if (!check(parser, type)) return false;
  advance(parser, scanner);
  return true;
}

static void emitByte(Parser* parser, ByteChunk* compilingChunk, uint8_t byte) {
  writeChunk(currentChunk(compilingChunk), byte, parser->previous.line);
}

static void emitBytes(Parser* parser, ByteChunk* compilingChunk, uint8_t byte1, uint8_t byte2) {
  emitByte(parser, compilingChunk, byte1);
  emitByte(parser, compilingChunk, byte2);
}

static void emitLoop(Parser* parser, ByteChunk* compilingChunk, int loopStart) {
  emitByte(parser, compilingChunk, OP_LOOP);

  int offset = compilingChunk->count - loopStart + 2;
  if (offset > UINT16_MAX) error(parser, "Loop body too large.");

  emitByte(parser, compilingChunk, (offset >> 8) & 0xff);
  emitByte(parser, compilingChunk, offset & 0xff);
}

static int emitJump(Parser* parser, ByteChunk* compilingChunk, uint8_t instruction) {
  emitByte(parser, compilingChunk, instruction);
  emitByte(parser, compilingChunk, 0xFF);
  emitByte(parser, compilingChunk, 0xFF);
  return compilingChunk->count - 2;
}

static void emitReturn(Parser* parser, ByteChunk* compilingChunk) {
  emitByte(parser, compilingChunk, OP_RETURN);
}

static void emitConstant(Parser* parser, ByteChunk* compilingChunk, Value value) {
  writeConstant(currentChunk(compilingChunk), value, parser->previous.line);
}

static void patchJump(Parser* parser, ByteChunk* compilingChunk, int offset) {
  int jump = compilingChunk->count - offset - 2;

  if (jump > UINT16_MAX) {
    error(parser, "Too much code to jump over.");
  }

  compilingChunk->code[offset] = (jump >> 8) & 0xFF;
  compilingChunk->code[offset + 1] = jump & 0xFF;
}

static void initCompiler(Compiler* compiler) {
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
}

static void endCompiler(Parser* parser, ByteChunk* compilingChunk) {
  emitReturn(parser, compilingChunk);
#ifdef DEBUG_PRINT_CODE
  if (!parser->hadError) {
    disassembleChunk(currentChunk(compilingChunk), "code");
  }
#endif /* ifdef DEBUG_PRINT_CODE */
}

static void beginScope(Compiler* currentCompiler) {
  currentCompiler->scopeDepth++;
}

static void endScope(Compiler* currentCompiler, Parser* parser, ByteChunk* compilingChunk) {
  currentCompiler->scopeDepth--;

  while (currentCompiler->localCount > 0 && 
      currentCompiler->locals[currentCompiler->localCount - 1].depth > currentCompiler->scopeDepth
  ) {
    emitByte(parser, compilingChunk, OP_POP);
    currentCompiler->localCount--;
  }
}

// Forward Declaration
static void expression(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk);
static void statement(VirtualMachine* vm, Compiler* compiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk);
static int identifierConstant(VirtualMachine* vm, ByteChunk* compilingChunk, Token* name);
static void declaration(VirtualMachine* vm, Compiler* compiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, Precedence precedence);

static int identifierConstant(VirtualMachine* vm, ByteChunk* compilingChunk, Token* name) {
  // Can potentiall use createConstantString
  ObjString* nameString = createString(vm, name->start, name->length); 
  Value value;
  // finding 
  for (int i = 0; i < compilingChunk->constants.count; i++) {
    if (valuesEqual(compilingChunk->constants.values[i], OBJECT_VAL(nameString))) {
      return i;
    }
  }
  addConstant(compilingChunk, OBJECT_VAL(nameString));
  return compilingChunk->constants.count - 1;
}

static bool identifiersEqual(Token* a, Token* b) {
  if (a->length != b->length) { return false; }
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* currentCompiler, Parser* parser, Token* name) {
  for (int i = currentCompiler->localCount - 1; i >= 0; i--) {
    Local* local = &currentCompiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error(parser, "Cannot read local variable in its own initializer.");
      }
      return i;
    }
  }
  return -1;
}

static void addLocal(Compiler* currentCompiler, Parser* parser, Token name) {
  if (currentCompiler->localCount == UINT8_COUNT) {
    error(parser, "Too many local variables in function/scope.");
    return;
  }
  Local* local = &currentCompiler->locals[currentCompiler->localCount++];
  local->name = name;
  local->depth = -1; 
}

static void declareVariable(Compiler* currentCompiler, Parser* parser) {
  if (currentCompiler->scopeDepth == 0) return;
  Token* name = &parser->previous;

  for (int i = currentCompiler->localCount - 1; i >= 0; i--) {
    Local* local = &currentCompiler->locals[i];
    if (local->depth != -1 && local->depth < currentCompiler->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error(parser, "Already a variable with this name in this scope.");
    }
  }

  addLocal(currentCompiler, parser, *name);
}

static uint8_t parseVariable(
    VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, 
    ByteChunk* compilingChunk, const char* errorMessage
) {
  consume(parser, scanner, TOKEN_IDENTIFIER, errorMessage);

  declareVariable(currentCompiler, parser);
  if (currentCompiler->scopeDepth > 0) { return 0; }

  return identifierConstant(vm, compilingChunk, &parser->previous);
}

static void markInitialized(Compiler* currentCompiler) {
  currentCompiler->locals[currentCompiler->localCount - 1].depth = currentCompiler->scopeDepth;
}

static void defineVariable(Compiler* currentCompiler, Parser* parser, ByteChunk* compilingChunk, uint8_t global) {
  if (currentCompiler->scopeDepth > 0) {
    markInitialized(currentCompiler);
    return;
  }
  emitBytes(parser, compilingChunk, OP_DEFINE_GLOBAL, global);
}

static void and_(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  int endJump = emitJump(parser, compilingChunk, OP_JUMP_IF_FALSE);

  emitByte(parser, compilingChunk, OP_POP);
  parsePrecedence(vm, currentCompiler, parser, scanner, compilingChunk, PREC_AND);

  patchJump(parser, compilingChunk, endJump);
}

static void binary(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  TokenType operatorType = parser->previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence(vm, currentCompiler, parser, scanner, compilingChunk, (Precedence) rule->precedence + 1);

  switch (operatorType) {
    case TOKEN_BANG_EQUAL:     emitBytes(parser, compilingChunk, OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:    emitByte(parser, compilingChunk, OP_EQUAL); break;
    case TOKEN_GREATER:        emitByte(parser, compilingChunk, OP_GREATER); break;
    case TOKEN_GREATER_EQUAL:  emitBytes(parser, compilingChunk, OP_LESS, OP_NOT); break;
    case TOKEN_LESS:           emitByte(parser, compilingChunk, OP_LESS); break;
    case TOKEN_LESS_EQUAL:     emitBytes(parser, compilingChunk, OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS:           emitByte(parser, compilingChunk, OP_ADD); break;
    case TOKEN_MINUS:          emitByte(parser, compilingChunk, OP_SUBTRACT); break;
    case TOKEN_STAR:           emitByte(parser, compilingChunk, OP_MULTIPLY); break;
    case TOKEN_SLASH:          emitByte(parser, compilingChunk, OP_DIVIDE); break;
    default: return; // unreachable
  }
}

static void literal(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  switch(parser->previous.type) {
    case TOKEN_FALSE: emitByte(parser, compilingChunk, OP_FALSE); break;
    case TOKEN_NAH: emitByte(parser, compilingChunk, OP_NAH); break;
    case TOKEN_TRUE: emitByte(parser, compilingChunk, OP_TRUE); break;
    default: return; // Unreachable 
  }
}

static void grouping(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  expression(vm, currentCompiler, parser, scanner, compilingChunk);
  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  double value = strtod(parser->previous.start, NULL);
  emitConstant(parser, compilingChunk, NUMBER_VAL(value));
} 

static void or_(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  int elseJump = emitJump(parser, compilingChunk, OP_JUMP_IF_FALSE);
  int endJump = emitJump(parser, compilingChunk, OP_JUMP);

  patchJump(parser, compilingChunk, elseJump);
  emitByte(parser, compilingChunk, OP_POP);

  parsePrecedence(vm, currentCompiler, parser, scanner, compilingChunk, PREC_OR);
  patchJump(parser, compilingChunk, endJump);
}

static void unary(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  TokenType operatorType = parser->previous.type;

  // Compile the operand;
  parsePrecedence(vm, currentCompiler, parser, scanner, compilingChunk, PREC_UNARY);

  // Emit the operator instruction.
  switch (operatorType) {
    case TOKEN_BANG:  emitByte(parser, compilingChunk, OP_NOT); break;
    case TOKEN_MINUS: emitByte(parser, compilingChunk, OP_NEGATE); break;
    default: return;
  }
}

static void string(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  emitConstant(
      parser, compilingChunk, 
      // OBJECT_VAL(createConstantString(vm, parser->previous.start + 1, parser->previous.length - 2))
      OBJECT_VAL(createString(vm, parser->previous.start + 1, parser->previous.length - 2))
  ); 
}

static void namedVariable(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign, Token name) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(currentCompiler, parser, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else {
    arg = identifierConstant(vm, compilingChunk, &name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
    expression(vm, currentCompiler, parser, scanner, compilingChunk);
    emitBytes(parser, compilingChunk, setOp, (uint8_t)arg);
  } else {
    emitBytes(parser, compilingChunk, getOp, (uint8_t)arg);
  }
}

static void variable(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  namedVariable(vm, currentCompiler, parser, scanner, compilingChunk, canAssign, parser->previous);
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]        = { grouping, NULL,     PREC_NONE },
  [TOKEN_RIGHT_PAREN]       = { NULL,     NULL,     PREC_NONE },
  [TOKEN_LEFT_BRACE]        = { NULL,     NULL,     PREC_NONE },
  [TOKEN_RIGHT_BRACE]       = { NULL,     NULL,     PREC_NONE },
  [TOKEN_COMMA]             = { NULL,     NULL,     PREC_NONE },
  [TOKEN_DOT]               = { NULL,     NULL,     PREC_NONE },
  [TOKEN_MINUS]             = { unary,    binary,   PREC_TERM },
  [TOKEN_PLUS]              = { NULL,     binary,   PREC_TERM },
  [TOKEN_SEMICOLON]         = { NULL,     NULL,     PREC_NONE },
  [TOKEN_SLASH]             = { NULL,     binary,   PREC_FACTOR },
  [TOKEN_STAR]              = { NULL,     binary,   PREC_FACTOR },
  [TOKEN_BANG]              = { unary,    NULL,     PREC_NONE },
  [TOKEN_BANG_EQUAL]        = { NULL,     binary,   PREC_EQUALITY },
  [TOKEN_EQUAL]             = { NULL,     NULL,     PREC_NONE },
  [TOKEN_EQUAL_EQUAL]       = { NULL,     binary,   PREC_EQUALITY },
  [TOKEN_GREATER]           = { NULL,     binary,   PREC_COMPARISON },
  [TOKEN_GREATER_EQUAL]     = { NULL,     binary,   PREC_COMPARISON },
  [TOKEN_LESS]              = { NULL,     binary,   PREC_COMPARISON },
  [TOKEN_LESS_EQUAL]        = { NULL,     binary,   PREC_COMPARISON },
  [TOKEN_IDENTIFIER]        = { variable, NULL,     PREC_NONE },
  [TOKEN_STRING]            = { string,   NULL,     PREC_NONE },
  [TOKEN_NUMBER]            = { number,   NULL,     PREC_NONE },
  [TOKEN_AND]               = { NULL,     and_,     PREC_AND  },
  [TOKEN_CLASS]             = { NULL,     NULL,     PREC_NONE },
  [TOKEN_ELSE]              = { NULL,     NULL,     PREC_NONE },
  [TOKEN_FALSE]             = { literal,  NULL,     PREC_NONE },
  [TOKEN_FOR]               = { NULL,     NULL,     PREC_NONE },
  [TOKEN_FUN]               = { NULL,     NULL,     PREC_NONE },
  [TOKEN_IF]                = { NULL,     NULL,     PREC_NONE },
  [TOKEN_NAH]               = { literal,  NULL,     PREC_NONE },
  [TOKEN_OR]                = { NULL,     or_,      PREC_OR },
  [TOKEN_PRINT]             = { NULL,     NULL,     PREC_NONE },
  [TOKEN_RETURN]            = { NULL,     NULL,     PREC_NONE },
  [TOKEN_SUPER]             = { NULL,     NULL,     PREC_NONE },
  [TOKEN_THIS]              = { NULL,     NULL,     PREC_NONE },
  [TOKEN_TRUE]              = { literal,  NULL,     PREC_NONE },
  [TOKEN_VAR]               = { NULL,     NULL,     PREC_NONE },
  [TOKEN_WHILE]             = { NULL,     NULL,     PREC_NONE },
  [TOKEN_ERROR]             = { NULL,     NULL,     PREC_NONE },
  [TOKEN_EOF]               = { NULL,     NULL,     PREC_NONE },
};

static void parsePrecedence(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, Precedence precedence) {
  advance(parser, scanner);
  ParseFn prefixRule = getRule(parser->previous.type)->prefix;
  if (prefixRule == NULL) {
    error(parser, "Expected expression.");
    return;
  }
  
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(vm, currentCompiler, parser, scanner, compilingChunk, canAssign);
  
  while (precedence <= getRule(parser->current.type)->precedence) {
    advance(parser, scanner);
    ParseFn infixRule = getRule(parser->previous.type)->infix;
    infixRule(vm, currentCompiler, parser, scanner, compilingChunk, canAssign);
    
    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
      error(parser, "Invalid assignment target.");
    }
  }

}


static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

static void expression(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  parsePrecedence(vm, currentCompiler, parser, scanner, compilingChunk, PREC_ASSIGNMENT);
}

static void block(VirtualMachine* vm, Compiler* compiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
    declaration(vm, compiler, parser, scanner, compilingChunk);
  }

  consume(parser, scanner, TOKEN_RIGHT_BRACE, "Expect '}' aafter block.");
}

static void varDeclaration(
    VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk
) {
  uint8_t global = parseVariable(vm, currentCompiler, parser, scanner, compilingChunk, "Expect variable name.");

  if (match(parser, scanner, TOKEN_EQUAL)) {
    expression(vm, currentCompiler, parser, scanner, compilingChunk);
  } else {
    emitByte(parser, compilingChunk, OP_NAH);
  }
  consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(currentCompiler, parser, compilingChunk, global);
}

static void expressionStatement(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  expression(vm, currentCompiler, parser, scanner, compilingChunk);
  consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(parser, compilingChunk, OP_POP);
}

static void ifStatement(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  consume(parser, scanner, TOKEN_LEFT_PAREN, "Expected '(' after 'if'.");
  expression(vm, currentCompiler, parser, scanner, compilingChunk);
  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(parser, compilingChunk, OP_JUMP_IF_FALSE);

  emitByte(parser, compilingChunk, OP_POP);

  statement(vm, currentCompiler, parser, scanner, compilingChunk);

  int elseJump = emitJump(parser, compilingChunk, OP_JUMP);
  patchJump(parser, compilingChunk, thenJump);
  
  emitByte(parser, compilingChunk, OP_POP);

  if (match(parser, scanner, TOKEN_ELSE)) {
    statement(vm, currentCompiler, parser, scanner, compilingChunk); 
  }
  patchJump(parser, compilingChunk, elseJump);
}

static void printStatement(
    VirtualMachine* vm, Compiler* currentCompiler, 
    Parser* parser, Scanner* scanner, ByteChunk* compilingChunk
) {
  expression(vm, currentCompiler, parser, scanner, compilingChunk);
  consume(parser, scanner, TOKEN_SEMICOLON, "Expected ';' after value.");
  emitByte(parser, compilingChunk, OP_PRINT);
}

static void whileStatement(
    VirtualMachine* vm, Compiler* currentCompiler, 
    Parser* parser, Scanner* scanner, ByteChunk* compilingChunk
) {
  int loopStart = compilingChunk->count;
  consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression(vm, currentCompiler, parser, scanner, compilingChunk);
  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(parser, compilingChunk, OP_JUMP_IF_FALSE);
  emitByte(parser, compilingChunk, OP_POP);

  statement(vm, currentCompiler, parser, scanner, compilingChunk);
  emitLoop(parser, compilingChunk, loopStart);

  patchJump(parser, compilingChunk, exitJump);
  emitByte(parser, compilingChunk, OP_POP);
}

static void synchronize(Parser* parser, Scanner* scanner) {
  parser->panicMode = false;

  while (parser->current.type != TOKEN_EOF) {
    if (parser->previous.type == TOKEN_SEMICOLON) return;
    switch (parser->current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;
      
      default: ; //init

      advance(parser, scanner);
  
    }
  }
}

static void declaration(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  if (match(parser, scanner, TOKEN_VAR)) {
    varDeclaration(vm, currentCompiler, parser, scanner, compilingChunk);
  } else {
    statement(vm, currentCompiler, parser, scanner, compilingChunk);
  } 

  if (parser->panicMode) synchronize(parser, scanner);
}

static void statement(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  if (match(parser, scanner, TOKEN_PRINT)) {
    printStatement(vm, currentCompiler, parser, scanner, compilingChunk);
  } else if (match(parser, scanner, TOKEN_IF)) {
    ifStatement(vm, currentCompiler, parser, scanner, compilingChunk);
  } else if (match(parser, scanner, TOKEN_WHILE)) {
    whileStatement(vm, currentCompiler, parser, scanner, compilingChunk);
  } else if (match(parser, scanner, TOKEN_LEFT_BRACE)) {
    beginScope(currentCompiler);
    block(vm, currentCompiler, parser, scanner, compilingChunk);
    endScope(currentCompiler, parser, compilingChunk);
  }else {
    expressionStatement(vm, currentCompiler, parser, scanner, compilingChunk);
  }
}

bool compile(VirtualMachine* vm, ByteChunk* chunk, const char* source) {
  Scanner scanner;
  initScanner(&scanner, source);

  Parser parser;
  parser.hadError = false;
  parser.panicMode = false;
  
  Compiler compiler;
  initCompiler(&compiler);

  ByteChunk* compilingChunk = chunk;

  advance(&parser, &scanner);

  while (!match(&parser, &scanner, TOKEN_EOF)) {
    declaration(vm, &compiler, &parser, &scanner, compilingChunk);
  }

  endCompiler(&parser, compilingChunk);

  return !parser.hadError;
}

