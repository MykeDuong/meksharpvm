#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "bytechunk.h"
#include "compiler.h"
#include "common.h"
#include "scanner.h"

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
}

static void parsePrecedence(Precedence precedence) {
  
}

static void binary(Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  TokenType operatorType = parser.previous.type;
  ParserRule* rule = getRule(operatorType);
  parsePrecedence((Precedence) rule->precedence + 1);

  switch (operatorType) {
    case TOKEN_PLUS:       emitByte(parser, compilingChunk, OP_ADD); break;
    case TOKEN_MINUS:      emitByte(parser, compilingChunk, OP_SUBTRACT); break;
    case TOKEN_STAR:       emitByte(parser, compilingChunk, OP_MULTIPLY); break;
    case TOKEN_SLASH:      emitByte(parser, compilingChunk, OP_DIVIDE); break;
    default: return; // unreachable
  }
}

static void grouping(Parser* parser, Scanner* scanner) {
  expression();
  consume(parser, scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  double value = strtod(parser->previous.start, NULL);
  emitConstant(parser, compilingChunk, value);
} 

static void unary(Parser* parser, Scanner* scanner, ByteChunk* compilingChunk) {
  TokenType operatorType = parser->previous.type;

  // Compile the operand;
  parsePrecedence(PREC_UNARY);

  // Emit the operator instruction.
  switch (operatorType) {
    case TOKEN_MINUS: emitByte(parser, compilingChunk, OP_NEGATE); break;
    default: return;
  }
}

static void expression() {
  parsePrecedence(PREC_ASSIGNMENT);
}




bool compile(const char *source, ByteChunk* chunk) {
  Scanner scanner;
  initScanner(&scanner, source);

  Parser parser;
  parser.hadError = false;
  parser.panicMode = false;
  
  ByteChunk* compilingChunk = chunk;

  advance(&parser, &scanner);
  expression();
  consume(&parser, &scanner, TOKEN_EOF, "Expect end of expression.");
  endCompiler(&parser, compilingChunk);

  return !parser.hadError;
}

