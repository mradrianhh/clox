#include <stdio.h>
#include <stdlib.h>

#include "compiler/compiler.h"
#include "compiler/scanner.h"
#include "core/object.h"

#ifdef DEBUG_PRINT_CODE
#include "core/debug.h"
#endif

typedef struct
{
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

typedef enum
{
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

typedef void (*ParseFn)();

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compiling_chunk;

static void Advance();
static void Expression();
static void Consume(TokenType type, const char *message);
static void EmitByte(uint8_t byte);
static void EmitReturn();
static void EmitBytes(uint8_t byte1, uint8_t byte2);
static void EmitConstant(Value value);
static uint8_t MakeConstant(Value value);
static Chunk *CurrentChunk();
static void EndCompiler();

static void ParsePrecedence(Precedence precedence);
static ParseRule *GetRule(TokenType type);
static void Number();
static void Grouping();
static void Unary();
static void Binary();
static void Literal();
static void String();

static void ErrorAtCurrent(const char *message);
static void ErrorAt(Token *token, const char *message);
static void Error(const char *message);

bool lox_Compile(const char *source, Chunk *chunk)
{
    lox_InitScanner(source);

    parser.panic_mode = false;
    parser.had_error = false;

    compiling_chunk = chunk;

    Advance();
    Expression();
    Consume(TOKEN_EOF, "Expect end of expression.");
    EndCompiler();
    return !parser.had_error;
}

void ParsePrecedence(Precedence precedence)
{
    Advance();
    ParseFn prefix_rule = GetRule(parser.previous.type)->prefix;
    if (prefix_rule == NULL)
    {
        Error("Expect expression.");
        return;
    }

    prefix_rule();

    while (precedence <= GetRule(parser.current.type)->precedence)
    {
        Advance();
        ParseFn infix_rule = GetRule(parser.previous.type)->infix;
        infix_rule();
    }
}

void Expression()
{
    ParsePrecedence(PREC_ASSIGNMENT);
}

void Number()
{
    double value = strtod(parser.previous.start, NULL);
    EmitConstant(NUMBER_VAL(value));
}

void Grouping()
{
    Expression();
    Consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

void Unary()
{
    TokenType operator_type = parser.previous.type;

    // Compile the operand.
    ParsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operator_type)
    {
    case TOKEN_MINUS:
        EmitByte(OP_NEGATE);
        break;
    case TOKEN_BANG:
        EmitByte(OP_NOT);
        break;
    default:
        return; // Unreachable.
    }
}

void Binary()
{
    TokenType operator_type = parser.previous.type;
    ParseRule *rule = GetRule(operator_type);
    ParsePrecedence((Precedence)(rule->precedence + 1));

    switch (operator_type)
    {
    case TOKEN_PLUS:
        EmitByte(OP_ADD);
        break;
    case TOKEN_MINUS:
        EmitByte(OP_SUBTRACT);
        break;
    case TOKEN_STAR:
        EmitByte(OP_MULTIPLY);
        break;
    case TOKEN_SLASH:
        EmitByte(OP_DIVIDE);
        break;
    case TOKEN_BANG_EQUAL:
        EmitBytes(OP_EQUAL, OP_NOT);
        break;
    case TOKEN_EQUAL_EQUAL:
        EmitByte(OP_EQUAL);
        break;
    case TOKEN_GREATER:
        EmitByte(OP_GREATER);
        break;
    case TOKEN_GREATER_EQUAL:
        EmitBytes(OP_LESS, OP_NOT);
        break;
    case TOKEN_LESS:
        EmitByte(OP_LESS);
        break;
    case TOKEN_LESS_EQUAL:
        EmitBytes(OP_GREATER, OP_NOT);
        break;
    default:
        return; // Unreachable.
    }
}

void Literal()
{
    switch (parser.previous.type)
    {
    case TOKEN_FALSE:
        EmitByte(OP_FALSE);
        break;
    case TOKEN_NIL:
        EmitByte(OP_NIL);
        break;
    case TOKEN_TRUE:
        EmitByte(OP_TRUE);
        break;
    default:
        return; // Unreachable.
    }
}

void String()
{
    EmitConstant(OBJ_VAL(lox_CopyString(parser.previous.start + 1,
                                    parser.previous.length - 2)));
}

void Advance()
{
    parser.previous = parser.current;

    for (;;)
    {
        parser.current = lox_ScanToken();
        if (parser.current.type != TOKEN_ERROR)
        {
            break;
        }

        ErrorAtCurrent(parser.current.start);
    }
}

void Consume(TokenType type, const char *message)
{
    if (parser.current.type == type)
    {
        Advance();
        return;
    }

    ErrorAtCurrent(message);
}

void EmitByte(uint8_t byte)
{
    lox_WriteChunk(CurrentChunk(), byte, parser.previous.line);
}

void EmitReturn()
{
    EmitByte(OP_RETURN);
}

void EmitBytes(uint8_t byte1, uint8_t byte2)
{
    EmitByte(byte1);
    EmitByte(byte2);
}

void EmitConstant(Value value)
{
    EmitBytes(OP_CONSTANT, MakeConstant(value));
}

uint8_t MakeConstant(Value value)
{
    int constant = lox_AddConstant(CurrentChunk(), value);
    if (constant > UINT8_MAX)
    {
        Error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

Chunk *CurrentChunk()
{
    return compiling_chunk;
}

void EndCompiler()
{
    EmitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error)
    {
        lox_DisassembleChunk(CurrentChunk(), "code");
    }
#endif
}

void ErrorAtCurrent(const char *message)
{
    ErrorAt(&parser.current, message);
}

void ErrorAt(Token *token, const char *message)
{
    if (parser.panic_mode)
        return;
    parser.panic_mode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // Nothing.
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

void Error(const char *message)
{
    ErrorAt(&parser.previous, message);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {Grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {Unary, Binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, Binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, Binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, Binary, PREC_FACTOR},
    [TOKEN_BANG] = {Unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, Binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, Binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, Binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, Binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, Binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, Binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {String, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {Number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {Literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {Literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {Literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

ParseRule *GetRule(TokenType type)
{
    return &rules[type];
}
