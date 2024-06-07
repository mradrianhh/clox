#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef void (*ParseFn)(bool can_assign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct
{
    Token name;
    int depth;
} Local;

typedef struct
{
    Local locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
} Compiler;

Parser parser;
Compiler *current = NULL;
Chunk *compiling_chunk;

static void Advance();
static void Consume(TokenType type, const char *message);
static void EmitByte(uint8_t byte);
static void EmitReturn();
static void EmitBytes(uint8_t byte1, uint8_t byte2);
static void EmitConstant(Value value);
static int EmitJump(uint8_t instruction);
static void PatchJump(int offset);
static uint8_t MakeConstant(Value value);
static Chunk *CurrentChunk();
static void EndCompiler();
static bool Match(TokenType type);
static bool Check(TokenType type);
static void AddLocal(Token name);

static void ParsePrecedence(Precedence precedence);
static ParseRule *GetRule(TokenType type);
static uint8_t ParseVariable(const char *err_msg);
static void DeclareVariable();
static void DefineVariable(uint8_t global);
static void NamedVariable(Token name, bool can_assign);
static void BeginScope();
static void EndScope();

static void Declaration();
static void VariableDeclaration();

static void Statement();
static void PrintStatement();
static void ExpressionStatement();
static void Block();
static void IfStatement();

static void Expression();

static void Number(bool can_assign);
static void Grouping(bool can_assign);
static void Unary(bool can_assign);
static void Binary(bool can_assign);
static void Literal(bool can_assign);
static void String(bool can_assign);
static void VariableReference(bool can_assign);

static void Synchronize();
static void ErrorAtCurrent(const char *message);
static void ErrorAt(Token *token, const char *message);
static void Error(const char *message);

static void InitCompiler(Compiler *compiler)
{
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    current = compiler;
}

bool lox_Compile(const char *source, Chunk *chunk)
{
    lox_InitScanner(source);

    Compiler compiler;
    InitCompiler(&compiler);

    parser.panic_mode = false;
    parser.had_error = false;

    compiling_chunk = chunk;

    Advance();
    while (!Match(TOKEN_EOF))
    {
        Declaration();
    }
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

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= GetRule(parser.current.type)->precedence)
    {
        Advance();
        ParseFn infix_rule = GetRule(parser.previous.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && Match(TOKEN_EQUAL))
    {
        Error("Invalid assignment target.");
    }
}

void Declaration()
{
    if (Match(TOKEN_VAR))
    {
        VariableDeclaration();
    }
    else
    {
        Statement();
    }

    if (parser.panic_mode)
        Synchronize();
}

void VariableDeclaration()
{
    uint8_t global = ParseVariable("Expect variable name.");

    if (Match(TOKEN_EQUAL))
    {
        Expression();
    }
    else
    {
        EmitByte(OP_NIL);
    }

    Consume(TOKEN_SEMICOLON,
            "Expect ';' after variable declaration.");

    DefineVariable(global);
}

void Statement()
{
    if (Match(TOKEN_PRINT))
    {
        PrintStatement();
    }
    else if (Match(TOKEN_LEFT_BRACE))
    {
        BeginScope();
        Block();
        EndScope();
    }
    else if (Match(TOKEN_IF))
    {
        IfStatement();
    }
    else
    {
        ExpressionStatement();
    }
}

void PrintStatement()
{
    Expression();
    Consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    EmitByte(OP_PRINT);
}

void ExpressionStatement()
{
    Expression();
    Consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    EmitByte(OP_POP);
}

void Block()
{
    while (!Check(TOKEN_RIGHT_BRACE) && !Check(TOKEN_EOF))
    {
        Declaration();
    }

    Consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

void IfStatement()
{
    // When compiling an if-statement, we will place an OP_JUMP_IF_FALSE at the beginning of the 
    // then-statements, so that the then-statement is skipped if the condition evaluates to false.
    // We will also place an OP_JUMP instruction at the end of the then-statement
    // that skips the else-statement if the condition evaluates to true.

    Consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    Expression();
    Consume(TOKEN_RIGHT_PAREN, "Expect ')' after 'if'-condition.");

    // Use backpatching to hold a temporary offset until we've compiled the
    // then-statement. 
    int then_jump = EmitJump(OP_JUMP_IF_FALSE);

    // Right after OP_JUMP_IF_FALSE, we add OP_POP to pop the condition if it evaluated to true.
    // Note: OP_POP will only be executed here if the condition evaluates to true because it follows the
    //       OP_JUMP_IF_FALSE instruction.
    EmitByte(OP_POP);
    
    Statement();

    // After compiling the then-statement, we need to prepare an else-jump regardless of whether
    // the user wrote an else-clause. This is to prevent the VM from executing the else-clause after the
    // then-clause if the condition is true.
    int else_jump = EmitJump(OP_JUMP);

    // Right after OP_JUMP, we also add OP_POP to pop the condition if it evaluated to false.
    EmitByte(OP_POP);

    // When the then-statement is compiled, we patch it with the now-known offset.
    PatchJump(then_jump);

    if(Match(TOKEN_ELSE))
    {
        Statement();
    }
    
    PatchJump(else_jump);
}

void Expression()
{
    ParsePrecedence(PREC_ASSIGNMENT);
}

void Number(bool can_assign)
{
    double value = strtod(parser.previous.start, NULL);
    EmitConstant(NUMBER_VAL(value));
}

void Grouping(bool can_assign)
{
    Expression();
    Consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

void Unary(bool can_assign)
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

void Binary(bool can_assign)
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

void Literal(bool can_assign)
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

void String(bool can_assign)
{
    EmitConstant(OBJ_VAL(lox_CopyString(parser.previous.start + 1,
                                        parser.previous.length - 2)));
}

void VariableReference(bool can_assign)
{
    NamedVariable(parser.previous, can_assign);
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

int EmitJump(uint8_t instruction)
{
    EmitByte(instruction);
    // Temporary offset-placeholder.
    EmitByte(0xFF);
    EmitByte(0xFF);
    // Return the offset of the jump-instruction.
    return CurrentChunk()->count - 2;
}

void PatchJump(int offset)
{
    int jump = CurrentChunk()->count - offset - 2;

    if(jump > UINT16_MAX)
    {
        Error("Max offset length of jump-instruction exceeded");
    }

    // Sets the jump-offset so that it points to the instruction following the then-statement.
    CurrentChunk()->code[offset] = (jump >> 8) & 0xFF;
    CurrentChunk()->code[offset + 1] = jump & 0xFF;
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

bool Match(TokenType type)
{
    if (!Check(type))
        return false;
    Advance();
    return true;
}

bool Check(TokenType type)
{
    return parser.current.type == type;
}

void AddLocal(Token name)
{
    if (current->local_count == UINT8_COUNT)
    {
        Error("Too many local variables in function.");
        return;
    }

    Local *local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
}

void Synchronize()
{
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser.current.type)
        {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;

        default:; // Do nothing.
        }

        Advance();
    }
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
    [TOKEN_IDENTIFIER] = {VariableReference, NULL, PREC_NONE},
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

static uint8_t IdentifierConstant(Token *name)
{
    return MakeConstant(OBJ_VAL(lox_CopyString(name->start, name->length)));
}

static bool IdentifiersEqual(Token *a, Token *b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int ResolveLocal(Compiler *compiler, Token *name)
{
    for (int i = compiler->local_count - 1; i >= 0; i--)
    {
        Local *local = &compiler->locals[i];
        if (IdentifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
            {
                Error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static void MarkInitialized()
{
    current->locals[current->local_count - 1].depth = current->scope_depth;
}

uint8_t ParseVariable(const char *err_msg)
{
    Consume(TOKEN_IDENTIFIER, err_msg);

    DeclareVariable();
    if (current->scope_depth > 0)
        return 0;

    return IdentifierConstant(&parser.previous);
}

void DeclareVariable()
{
    if (current->scope_depth == 0)
        return;

    Token *name = &parser.previous;

    for (int i = current->local_count - 1; i >= 0; i--)
    {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth)
        {
            break;
        }

        if (IdentifiersEqual(name, &local->name))
        {
            Error("Already a variable with this name in this scope.");
        }
    }

    AddLocal(*name);
}

void DefineVariable(uint8_t global)
{
    if (current->scope_depth > 0)
    {
        MarkInitialized();
        return;
    }

    EmitBytes(OP_DEFINE_GLOBAL, global);
}

void NamedVariable(Token name, bool can_assign)
{
    uint8_t get_op, set_op;
    int arg = ResolveLocal(current, &name);
    if (arg != -1)
    {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    }
    else
    {
        arg = IdentifierConstant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && Match(TOKEN_EQUAL))
    {
        Expression();
        EmitBytes(set_op, (uint8_t)arg);
    }
    else
    {
        EmitBytes(get_op, (uint8_t)arg);
    }
}

void BeginScope()
{
    current->scope_depth++;
}

void EndScope()
{
    current->scope_depth--;

    // Pop all locals on stack within ended scope.
    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth >
               current->scope_depth)
    {
        EmitByte(OP_POP);
        current->local_count--;
    }
}
