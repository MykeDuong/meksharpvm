#include <stdio.h>
#include <stdlib.h>

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

static void emitReturn(Parser* parser, ByteChunk* compilingChunk) {
  emitByte(parser, compilingChunk, OP_RETURN);
}

static void emitConstant(Parser* parser, ByteChunk* compilingChunk, Value value) {
  writeConstant(currentChunk(compilingChunk), value, parser->previous.line);
}

static void endCompiler(Parser* parser, ByteChunk* compilingChunk) {
  emitReturn(parser, compilingChunk);
#ifdef DEBUG_PRINT_CODE
  if (!parser->hadError) {
    disassembleChunk(currentChunk(compilingChunk), "code");
  }
#endif /* ifdef DEBUG_PRINT_CODE */
}

// Forward Declaration
static void expression(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk);
static void statement(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk);
static int identifierConstant(VirtualMachine* vm, ByteChunk* compilingChunk, Token* name);
static void declaration(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, Precedence precedence);

static void binary(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  TokenType operatorType = parser->previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence(vm, parser, scanner, compilingChunk, (Precedence) rule->precedence + 1);

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

static void literal(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  switch(parser->previous.type) {
    case TOKEN_FALSE: emitByte(parser, compilingChunk, OP_FALSE); break;
    case TOKEN_NAH: emitByte(parser, compilingChunk, OP_NAH); break;
    case TOKEN_TRUE: emitByte(parser, compilingChunk, OP_TRUE); break;
    default: return; // Unreachable 
  }
}

static void grouping(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  expression(vm, parser, scanner, compilingChunk);
  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  double value = strtod(parser->previous.start, NULL);
  emitConstant(parser, compilingChunk, NUMBER_VAL(value));
} 

static void unary(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  TokenType operatorType = parser->previous.type;

  // Compile the operand;
  parsePrecedence(vm, parser, scanner, compilingChunk, PREC_UNARY);

  // Emit the operator instruction.
  switch (operatorType) {
    case TOKEN_BANG:  emitByte(parser, compilingChunk, OP_NOT); break;
    case TOKEN_MINUS: emitByte(parser, compilingChunk, OP_NEGATE); break;
    default: return;
  }
}

static void string(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  emitConstant(
      parser, compilingChunk, 
      // OBJECT_VAL(createConstantString(vm, parser->previous.start + 1, parser->previous.length - 2))
      OBJECT_VAL(createString(vm, parser->previous.start + 1, parser->previous.length - 2))
  ); 
}

static void namedVariable(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign, Token name) {
  uint8_t arg = identifierConstant(vm, compilingChunk, &name);
  if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
    expression(vm, parser, scanner, compilingChunk);
    emitBytes(parser, compilingChunk, OP_SET_GLOBAL, arg);
  } else {
    emitBytes(parser, compilingChunk, OP_GET_GLOBAL, arg);
  }
}

static void variable(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, bool canAssign) {
  namedVariable(vm, parser, scanner, compilingChunk, canAssign, parser->previous);
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
  [TOKEN_AND]               = { NULL,     NULL,     PREC_NONE },
  [TOKEN_CLASS]             = { NULL,     NULL,     PREC_NONE },
  [TOKEN_ELSE]              = { NULL,     NULL,     PREC_NONE },
  [TOKEN_FALSE]             = { literal,  NULL,     PREC_NONE },
  [TOKEN_FOR]               = { NULL,     NULL,     PREC_NONE },
  [TOKEN_FUN]               = { NULL,     NULL,     PREC_NONE },
  [TOKEN_IF]                = { NULL,     NULL,     PREC_NONE },
  [TOKEN_NAH]               = { literal,  NULL,     PREC_NONE },
  [TOKEN_OR]                = { NULL,     NULL,     PREC_NONE },
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

static void parsePrecedence(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, Precedence precedence) {
  advance(parser, scanner);
  ParseFn prefixRule = getRule(parser->previous.type)->prefix;
  if (prefixRule == NULL) {
    error(parser, "Expected expression.");
    return;
  }
  
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(vm, parser, scanner, compilingChunk, canAssign);
  
  while (precedence <= getRule(parser->current.type)->precedence) {
    advance(parser, scanner);
    ParseFn infixRule = getRule(parser->previous.type)->infix;
    infixRule(vm, parser, scanner, compilingChunk, canAssign);
    
    if (canAssign && match(parser, scanner, TOKEN_EQUAL)) {
      error(parser, "Invalid assignment target.");
    }
  }

}

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

static uint8_t parseVariable(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk, const char* errorMessage) {
  consume(parser, scanner, TOKEN_IDENTIFIER, errorMessage);
  return identifierConstant(vm, compilingChunk, &parser->previous);
}

static void defineVariable(Parser* parser, ByteChunk* compilingChunk, uint8_t global) {
  emitBytes(parser, compilingChunk, OP_DEFINE_GLOBAL, global);
}

static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

static void expression(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  parsePrecedence(vm, parser, scanner, compilingChunk, PREC_ASSIGNMENT);
}

static void varDeclaration(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  uint8_t global = parseVariable(vm, parser, scanner, compilingChunk, "Expect variable name");

  if (match(parser, scanner, TOKEN_EQUAL)) {
    expression(vm, parser, scanner, compilingChunk);
  } else {
    emitByte(parser, compilingChunk, OP_NAH);
  }
  consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration");

  defineVariable(parser,compilingChunk, global);
}

static void expressionStatement(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  expression(vm, parser, scanner, compilingChunk);
  consume(parser, scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(parser, compilingChunk, OP_POP);
}

static void printStatement(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  expression(vm, parser, scanner, compilingChunk);
  consume(parser, scanner, TOKEN_SEMICOLON, "Expected ';' after value.");
  emitByte(parser, compilingChunk, OP_PRINT);
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

static void declaration(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  if (match(parser, scanner, TOKEN_VAR)) {
    varDeclaration(vm, parser, scanner, compilingChunk);
  } else {
    statement(vm, parser, scanner, compilingChunk);
  } 

  if (parser->panicMode) synchronize(parser, scanner);
}

static void statement(VirtualMachine* vm, Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  if (match(parser, scanner, TOKEN_PRINT)) {
    printStatement(vm, parser, scanner, compilingChunk);
  } else {
    expressionStatement(vm, parser, scanner, compilingChunk);
  }
}

bool compile(VirtualMachine* vm, ByteChunk* chunk, const char* source) {
  Scanner scanner;
  initScanner(&scanner, source);

  Parser parser;
  parser.hadError = false;
  parser.panicMode = false;
  
  ByteChunk* compilingChunk = chunk;

  advance(&parser, &scanner);

  while (!match(&parser, &scanner, TOKEN_EOF)) {
    declaration(vm, &parser, &scanner, compilingChunk);
  }

  endCompiler(&parser, compilingChunk);

  return !parser.hadError;
}

