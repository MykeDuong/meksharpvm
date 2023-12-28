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
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
  struct Compiler* enclosing;
  ObjFunction* function;
  FunctionType type;

  Local locals[UINT8_COUNT];
  int localCount;
  Upvalue upvalues[UINT8_COUNT];
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
  bool canAssign
);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;



static ByteChunk* currentChunk(Compiler* currentCompiler) {
  return &currentCompiler->function->chunk;
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

static void emitByte(Compiler* currentCompiler, Parser* parser, uint8_t byte) {
  writeChunk(currentChunk(currentCompiler), byte, parser->previous.line);
}

static void emitBytes(Compiler* currentCompiler, Parser* parser, uint8_t byte1, uint8_t byte2) {
  emitByte(currentCompiler, parser, byte1);
  emitByte(currentCompiler, parser, byte2);
}

static void emitLoop(Compiler* currentCompiler, Parser* parser, int loopStart) {
  emitByte(currentCompiler, parser, OP_LOOP);

  int offset = currentChunk(currentCompiler)->count - loopStart + 2;
  if (offset > UINT16_MAX) error(parser, "Loop body too large.");

  emitByte(currentCompiler, parser, (offset >> 8) & 0xff);
  emitByte(currentCompiler, parser, offset & 0xff);
}

static int emitJump(Compiler* currentCompiler, Parser* parser, uint8_t instruction) {
  emitByte(currentCompiler, parser, instruction);
  emitByte(currentCompiler, parser, 0xFF);
  emitByte(currentCompiler, parser, 0xFF);
  return currentChunk(currentCompiler)->count - 2;
}

static void emitReturn(Compiler* currentCompiler, Parser* parser) {
  emitByte(currentCompiler, parser, OP_NAH);
  emitByte(currentCompiler, parser, OP_RETURN);
}

static void emitConstant(Compiler* currentCompiler, Parser* parser, Value value) {
  writeConstant(currentChunk(currentCompiler), value, parser->previous.line);
}

static void patchJump(Compiler* currentCompiler, Parser* parser, int offset) {
  int jump = currentChunk(currentCompiler)->count - offset - 2;

  if (jump > UINT16_MAX) {
    error(parser, "Too much code to jump over.");
  }

  currentChunk(currentCompiler)->code[offset] = (jump >> 8) & 0xFF;
  currentChunk(currentCompiler)->code[offset + 1] = jump & 0xFF;
}

static void initCompiler(VirtualMachine* vm, Parser* parser, Compiler* compiler, Compiler* enclosing, FunctionType type) {
  compiler->enclosing = enclosing;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->function = newFunction(vm);

  if (type != TYPE_SCRIPT) {
    compiler->function->name = createString(vm, parser->previous.start, parser->previous.length);
  }

  Local* local = &compiler->locals[compiler->localCount++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
}

static ObjFunction* endCompiler(Compiler* currentCompiler, Parser* parser) {
  emitReturn(currentCompiler, parser);
  ObjFunction* function = currentCompiler->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser->hadError) {
    disassembleChunk(currentChunk(currentCompiler), function->name != NULL ? function->name->chars : "<script>");
  }
#endif /* ifdef DEBUG_PRINT_CODE */
  return function;
}

static void beginScope(Compiler* currentCompiler) {
  currentCompiler->scopeDepth++;
}

static void endScope(Compiler* currentCompiler, Parser* parser) {
  currentCompiler->scopeDepth--;

  while (currentCompiler->localCount > 0 && 
      currentCompiler->locals[currentCompiler->localCount - 1].depth > currentCompiler->scopeDepth
  ) {
    emitByte(currentCompiler, parser, OP_POP);
    currentCompiler->localCount--;
  }
}

// Forward Declaration
static void expression(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner);
static void statement(VirtualMachine* vm, Compiler* compiler, Parser* parser, Scanner* scanner);
static void declaration(VirtualMachine* vm, Compiler* compiler, Parser* parser, Scanner* scanner);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, Precedence precedence);

static int identifierConstant(Compiler* currentCompiler, VirtualMachine* vm, Token* name) {
  // Can potentiall use createConstantString
  ObjString* nameString = createString(vm, name->start, name->length); 
  Value value;
  // finding 
  for (int i = 0; i < currentChunk(currentCompiler)->constants.count; i++) {
    if (valuesEqual(currentChunk(currentCompiler)->constants.values[i], OBJECT_VAL(nameString))) {
      return i;
    }
  }
  addConstant(currentChunk(currentCompiler), OBJECT_VAL(nameString));
  return currentChunk(currentCompiler)->constants.count - 1;
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

static int addUpvalue(Compiler* currentCompiler, Parser* parser, uint8_t index, bool isLocal) {
  int upvalueCount = currentCompiler->function->upvalueCount;
  
  for (int i = 0; i < upvalueCount; i++) {
    Upvalue* upvalue = &currentCompiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      // already closed
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    error(parser, "Too many closure variables in function.");
    return 0;
  }

  currentCompiler->upvalues[upvalueCount].isLocal = isLocal;
  currentCompiler->upvalues[upvalueCount].index = index;
  return currentCompiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* currentCompiler, Parser* parser, Token* name) {
  if (currentCompiler->enclosing == NULL) return -1;
  int local = resolveLocal(currentCompiler->enclosing, parser, name);

  if (local != -1) {
    return addUpvalue(currentCompiler, parser, (uint8_t)local, true);
  }

  int upvalue = resolveUpvalue(currentCompiler->enclosing, parser, name);
  if (upvalue != -1) {
    return addUpvalue(currentCompiler, parser, (uint8_t)upvalue, false);
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
    const char* errorMessage
) {
  consume(parser, scanner, TOKEN_IDENTIFIER, errorMessage);

  declareVariable(currentCompiler, parser);
  if (currentCompiler->scopeDepth > 0) { return 0; }

  return identifierConstant(currentCompiler, vm, &parser->previous);
}

static void markInitialized(Compiler* currentCompiler) {
  if (currentCompiler->scopeDepth == 0) return;
  currentCompiler->locals[currentCompiler->localCount - 1].depth = currentCompiler->scopeDepth;
}

static void defineVariable(Compiler* currentCompiler, Parser* parser, uint8_t global) {
  if (currentCompiler->scopeDepth > 0) {
    markInitialized(currentCompiler);
    return;
  }
  emitBytes(currentCompiler, parser, OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner) {
  uint8_t argCount = 0;

  if (!check(parser, TOKEN_RIGHT_PAREN)) {
    do {
      expression(vm, currentCompiler, parser, scanner);
      if (argCount == 255) {
        error(parser, "Cannot have more than 255 arguments.");
      }
      argCount++;
    } while (match(parser, scanner, TOKEN_COMMA));
  }
  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}

static void and_(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  int endJump = emitJump(currentCompiler, parser, OP_JUMP_IF_FALSE);

  emitByte(currentCompiler, parser, OP_POP);
  parsePrecedence(vm, currentCompiler, parser, scanner, PREC_AND);

  patchJump(currentCompiler, parser, endJump);
}

static void binary(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  TokenType operatorType = parser->previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence(vm, currentCompiler, parser, scanner, (Precedence) rule->precedence + 1);

  switch (operatorType) {
    case TOKEN_BANG_EQUAL:     emitBytes(currentCompiler, parser, OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:    emitByte(currentCompiler, parser, OP_EQUAL); break;
    case TOKEN_GREATER:        emitByte(currentCompiler, parser, OP_GREATER); break;
    case TOKEN_GREATER_EQUAL:  emitBytes(currentCompiler, parser, OP_LESS, OP_NOT); break;
    case TOKEN_LESS:           emitByte(currentCompiler, parser, OP_LESS); break;
    case TOKEN_LESS_EQUAL:     emitBytes(currentCompiler, parser, OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS:           emitByte(currentCompiler, parser, OP_ADD); break;
    case TOKEN_MINUS:          emitByte(currentCompiler, parser, OP_SUBTRACT); break;
    case TOKEN_STAR:           emitByte(currentCompiler, parser, OP_MULTIPLY); break;
    case TOKEN_SLASH:          emitByte(currentCompiler, parser, OP_DIVIDE); break;
    default: return; // unreachable
  }
}

static void call(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  uint8_t argCount = argumentList(vm, currentCompiler, parser, scanner);
  emitBytes(currentCompiler, parser, OP_CALL, argCount);
}

static void literal(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  switch(parser->previous.type) {
    case TOKEN_FALSE: emitByte(currentCompiler, parser, OP_FALSE); break;
    case TOKEN_NAH: emitByte(currentCompiler, parser, OP_NAH); break;
    case TOKEN_TRUE: emitByte(currentCompiler, parser, OP_TRUE); break;
    default: return; // Unreachable 
  }
}

static void grouping(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  expression(vm, currentCompiler, parser, scanner);
  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  double value = strtod(parser->previous.start, NULL);
  emitConstant(currentCompiler, parser, NUMBER_VAL(value));
} 

static void or_(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  int elseJump = emitJump(currentCompiler, parser, OP_JUMP_IF_FALSE);
  int endJump = emitJump(currentCompiler, parser, OP_JUMP);

  patchJump(currentCompiler, parser, elseJump);
  emitByte(currentCompiler, parser, OP_POP);

  parsePrecedence(vm, currentCompiler, parser, scanner, PREC_OR);
  patchJump(currentCompiler, parser, endJump);
}

static void unary(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  TokenType operatorType = parser->previous.type;

  // Compile the operand;
  parsePrecedence(vm, currentCompiler, parser, scanner,  PREC_UNARY);

  // Emit the operator instruction.
  switch (operatorType) {
    case TOKEN_BANG:  emitByte(currentCompiler, parser, OP_NOT); break;
    case TOKEN_MINUS: emitByte(currentCompiler, parser, OP_NEGATE); break;
    default: return;
  }
}

static void string(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  emitConstant(
      currentCompiler, parser, 
      // OBJECT_VAL(createConstantString(vm, parser->previous.start + 1, parser->previous.length - 2))
      OBJECT_VAL(createString(vm, parser->previous.start + 1, parser->previous.length - 2))
  ); 
}

static void namedVariable(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign, Token name) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(currentCompiler, parser, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(currentCompiler, parser, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  }else {
    arg = identifierConstant(currentCompiler, vm, &name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
    expression(vm, currentCompiler, parser, scanner);
    emitBytes(currentCompiler, parser, setOp, (uint8_t)arg);
  } else {
    emitBytes(currentCompiler, parser, getOp, (uint8_t)arg);
  }
}

static void variable(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, bool canAssign) {
  namedVariable(vm, currentCompiler, parser, scanner, canAssign, parser->previous);
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]        = { grouping, call,     PREC_CALL },
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

static void parsePrecedence(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, Precedence precedence) {
  advance(parser, scanner);
  ParseFn prefixRule = getRule(parser->previous.type)->prefix;
  if (prefixRule == NULL) {
    error(parser, "Expected expression.");
    return;
  }
  
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(vm, currentCompiler, parser, scanner, canAssign);
  
  while (precedence <= getRule(parser->current.type)->precedence) {
    advance(parser, scanner);
    ParseFn infixRule = getRule(parser->previous.type)->infix;
    infixRule(vm, currentCompiler, parser, scanner,  canAssign);
    
    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
      error(parser, "Invalid assignment target.");
    }
  }

}


static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

static void expression(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner) {
  parsePrecedence(vm, currentCompiler, parser, scanner,  PREC_ASSIGNMENT);
}

static void block(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner) {
  while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
    declaration(vm, currentCompiler, parser, scanner);
  }

  consume(parser, scanner, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner, FunctionType type) {
  Compiler compiler;
  initCompiler(vm, parser, &compiler, currentCompiler, type);
  beginScope(&compiler);

  /* Begin using new compiler */
  consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after function name.");

  if (!check(parser, TOKEN_RIGHT_PAREN)) {
    do {
      compiler.function->arity++;
      if (compiler.function->arity > 255) {
        errorAtCurrent(parser, "Cannot have more than 255 parameters.");
      }
      uint8_t constant = parseVariable(vm, &compiler, parser, scanner, "Expect parameter name.");
      defineVariable(&compiler, parser, constant);
    } while (match(parser, scanner, TOKEN_COMMA));
  }

  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(parser, scanner, TOKEN_LEFT_BRACE, "Expect '{' before function body."); 
  block(vm, &compiler, parser, scanner);

  ObjFunction* function = endCompiler(&compiler, parser);
  /* End using new compiler */

  emitBytes(currentCompiler, parser, OP_CLOSURE, addConstant(currentChunk(currentCompiler), OBJECT_VAL(function)));
  
  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(currentCompiler, parser, currentCompiler->upvalues[i].isLocal ? 1 : 0);
    emitByte(currentCompiler, parser, currentCompiler->upvalues[i].index);
  }
}

static void funDeclaration(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner) {
  uint8_t global = parseVariable(vm, currentCompiler, parser, scanner, "Expect function name.");
  markInitialized(currentCompiler);
  function(vm, currentCompiler, parser, scanner, TYPE_FUNCTION);
  defineVariable(currentCompiler, parser, global);
}


static void varDeclaration(
    VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner
) {
  uint8_t global = parseVariable(vm, currentCompiler, parser, scanner, "Expect variable name.");

  if (match(parser, scanner, TOKEN_EQUAL)) {
    expression(vm, currentCompiler, parser, scanner);
  } else {
    emitByte(currentCompiler, parser, OP_NAH);
  }
  consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(currentCompiler, parser, global);
}

static void expressionStatement(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner) {
  expression(vm, currentCompiler, parser, scanner);
  consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(currentCompiler, parser, OP_POP);
}

static void forStatement(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner) {
  beginScope(currentCompiler);

  consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  
  /* Initializer clause */
  if (match(parser, scanner, TOKEN_SEMICOLON)) {
    // No initializer clause 
  } else if (match(parser, scanner, TOKEN_VAR)) {
    varDeclaration(vm, currentCompiler, parser, scanner);
  } else {
    expressionStatement(vm, currentCompiler, parser, scanner);
  }

  int loopStart = currentChunk(currentCompiler)->count;

  int exitJump = -1;

  /* Loop condition clause */
  if (!match(parser, scanner, TOKEN_SEMICOLON)) {
    expression(vm, currentCompiler, parser, scanner);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    // Jump out of loop if the condition is false
    exitJump = emitJump(currentCompiler, parser, OP_JUMP_IF_FALSE);
    emitByte(currentCompiler, parser, OP_POP);
  }

  if (!match(parser, scanner, TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(currentCompiler, parser, OP_JUMP);
    int incrementStart = currentChunk(currentCompiler)->count;
    expression(vm, currentCompiler, parser, scanner);
    emitByte(currentCompiler, parser, OP_POP);
    consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after 'for' clause.");

    emitLoop(currentCompiler, parser, loopStart);
    loopStart = incrementStart;
    patchJump(currentCompiler, parser, bodyJump);
  }

  statement(vm, currentCompiler, parser, scanner);
  emitLoop(currentCompiler, parser, loopStart);
 
  /* Patch the loop condition (exit) jump */
  if (exitJump != -1) {
    patchJump(currentCompiler, parser, exitJump);
    emitByte(currentCompiler, parser, OP_POP);
  }

  endScope(currentCompiler, parser);
}

static void ifStatement(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner) {
  consume(parser, scanner, TOKEN_LEFT_PAREN, "Expected '(' after 'if'.");
  expression(vm, currentCompiler, parser, scanner);
  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(currentCompiler, parser, OP_JUMP_IF_FALSE);

  emitByte(currentCompiler, parser, OP_POP);

  statement(vm, currentCompiler, parser, scanner);

  int elseJump = emitJump(currentCompiler, parser, OP_JUMP);
  patchJump(currentCompiler, parser, thenJump);
  
  emitByte(currentCompiler, parser, OP_POP);

  if (match(parser, scanner, TOKEN_ELSE)) {
    statement(vm, currentCompiler, parser, scanner); 
  }
  patchJump(currentCompiler, parser, elseJump);
}

static void printStatement(
    VirtualMachine* vm, Compiler* currentCompiler, 
    Parser* parser, Scanner* scanner
) {
  expression(vm, currentCompiler, parser, scanner);
  consume(parser, scanner, TOKEN_SEMICOLON, "Expected ';' after value.");
  emitByte(currentCompiler, parser, OP_PRINT);
}

static void returnStatement(
    VirtualMachine* vm, Compiler* currentCompiler, 
    Parser* parser, Scanner* scanner
) {
  if (currentCompiler->type == TYPE_SCRIPT) {
    error(parser, "Cannot return from top-level code.");
  }

  if (match(parser, scanner, TOKEN_SEMICOLON)) {
    emitReturn(currentCompiler, parser);
  } else {
    expression(vm, currentCompiler, parser, scanner);
    consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(currentCompiler, parser, OP_RETURN);
  }
}

static void whileStatement(
    VirtualMachine* vm, Compiler* currentCompiler, 
    Parser* parser, Scanner* scanner
) {
  int loopStart = currentChunk(currentCompiler)->count;
  consume(parser, scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression(vm, currentCompiler, parser, scanner);
  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(currentCompiler, parser, OP_JUMP_IF_FALSE);
  emitByte(currentCompiler, parser, OP_POP);

  statement(vm, currentCompiler, parser, scanner);
  emitLoop(currentCompiler, parser, loopStart);

  patchJump(currentCompiler, parser, exitJump);
  emitByte(currentCompiler, parser, OP_POP);
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

static void declaration(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner) {
  if (match(parser, scanner, TOKEN_FUN)) {
    funDeclaration(vm, currentCompiler, parser, scanner);
  } else if (match(parser, scanner, TOKEN_VAR)) {
    varDeclaration(vm, currentCompiler, parser, scanner);
  } else {
    statement(vm, currentCompiler, parser, scanner);
  } 

  if (parser->panicMode) synchronize(parser, scanner);
}

static void statement(VirtualMachine* vm, Compiler* currentCompiler, Parser* parser, Scanner* scanner) {
  if (match(parser, scanner, TOKEN_PRINT)) {
    printStatement(vm, currentCompiler, parser, scanner);
  } else if (match(parser, scanner, TOKEN_FOR)) {
    forStatement(vm, currentCompiler, parser, scanner);
  } else if (match(parser, scanner, TOKEN_IF)) {
    ifStatement(vm, currentCompiler, parser, scanner);
  } else if (match(parser, scanner, TOKEN_RETURN)) {
    returnStatement(vm, currentCompiler, parser, scanner);
  }else if (match(parser, scanner, TOKEN_WHILE)) {
    whileStatement(vm, currentCompiler, parser, scanner);
  } else if (match(parser, scanner, TOKEN_LEFT_BRACE)) {
    beginScope(currentCompiler);
    block(vm, currentCompiler, parser, scanner);
    endScope(currentCompiler, parser);
  }else {
    expressionStatement(vm, currentCompiler, parser, scanner);
  }
}

ObjFunction* compile(VirtualMachine* vm, const char* source) {
  Scanner scanner;
  initScanner(&scanner, source);

  Parser parser;
  parser.hadError = false;
  parser.panicMode = false;
  
  Compiler compiler;
  initCompiler(vm, &parser, &compiler, NULL, TYPE_SCRIPT);

  advance(&parser, &scanner);

  while (!match(&parser, &scanner, TOKEN_EOF)) {
    declaration(vm, &compiler, &parser, &scanner);
  }

  endCompiler(&compiler, &parser);

  ObjFunction* function = endCompiler(&compiler, &parser);
  return parser.hadError ? NULL : function;
}

