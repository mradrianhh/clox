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

typedef enum
{
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler Compiler;
struct Compiler
{
    Compiler *enclosing;
    ObjFunction *function;
    FunctionType type;
    Local locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
};

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
static void EmitLoop(int loop_start);
static uint8_t MakeConstant(Value value);
static Chunk *CurrentChunk();
static ObjFunction *EndCompiler();
static bool Match(TokenType type);
static bool Check(TokenType type);
static void AddLocal(Token name);
static void MarkInitialized();

static void ParsePrecedence(Precedence precedence);
static ParseRule *GetRule(TokenType type);
static uint8_t ParseVariable(const char *err_msg);
static void DeclareVariable();
static void DefineVariable(uint8_t global);
static void NamedVariable(Token name, bool can_assign);
static void BeginScope();
static void EndScope();
static void Function(FunctionType type);
static uint8_t ArgumentList();

static void Declaration();
static void VariableDeclaration();
static void FunctionDeclaration();

static void Statement();
static void PrintStatement();
static void ExpressionStatement();
static void Block();
static void IfStatement();
static void WhileStatement();
static void ForStatement();
static void ReturnStatement();

static void Expression();

static void Number(bool can_assign);
static void Grouping(bool can_assign);
static void Unary(bool can_assign);
static void Binary(bool can_assign);
static void Literal(bool can_assign);
static void String(bool can_assign);
static void VariableReference(bool can_assign);
static void And_(bool can_assign);
static void Or_(bool can_assign);
static void Call(bool can_assign);

static void Synchronize();
static void ErrorAtCurrent(const char *message);
static void ErrorAt(Token *token, const char *message);
static void Error(const char *message);

static void InitCompiler(Compiler *compiler, FunctionType type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = lox_CreateFunction();
    current = compiler;

    if (type != TYPE_SCRIPT)
    {
        current->function->name = lox_CopyString(parser.previous.start, parser.previous.length);
    }

    Local *local = &current->locals[current->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

ObjFunction *lox_Compile(const char *source)
{
    lox_InitScanner(source);

    Compiler compiler;
    InitCompiler(&compiler, TYPE_SCRIPT);

    parser.panic_mode = false;
    parser.had_error = false;

    Advance();
    while (!Match(TOKEN_EOF))
    {
        Declaration();
    }
    ObjFunction *function = EndCompiler();
    return parser.had_error ? NULL : function;
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
    else if (Match(TOKEN_FUN))
    {
        FunctionDeclaration();
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

void FunctionDeclaration()
{
    uint8_t global = ParseVariable("Expect function name.");
    MarkInitialized();
    Function(TYPE_FUNCTION);
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
    else if (Match(TOKEN_WHILE))
    {
        WhileStatement();
    }
    else if (Match(TOKEN_FOR))
    {
        ForStatement();
    }
    else if (Match(TOKEN_RETURN))
    {
        ReturnStatement();
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

    if (Match(TOKEN_ELSE))
    {
        Statement();
    }

    PatchJump(else_jump);
}

void WhileStatement()
{
    // Fetch the offset of the while-instruction so we can loop.
    int loop_start = CurrentChunk()->count;
    Consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    Expression();
    Consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exit_jump = EmitJump(OP_JUMP_IF_FALSE);
    // If the condition evaluates to true, we don't skip the body of the while-statement
    // and we have to emit OP_POP to clear the condition-value of the stack.
    EmitByte(OP_POP);
    // Then we parse the body of the while-statement.
    Statement();

    // Then we emit a loop to return to the start of the while-instruction
    // and re-evaluate the condition.
    EmitLoop(loop_start);

    // Backpatch exit_jump to point to the instruction following the body of the while-statement.
    PatchJump(exit_jump);
    // The first instruction following the while-statement is OP_POP to clear the
    // condition-value from the stack.
    EmitByte(OP_POP);
}

void ForStatement()
{
    // Create a scope for the for-statement to ensure variables declared in it's initalized
    // is scoped to the for-statement.
    BeginScope();

    // Initializer.
    Consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (Match(TOKEN_SEMICOLON))
    {
        // No initializer.
    }
    // Both VariableDeclaration and ExpressionStatement will check for a semicolon
    // and maintain the stack themselves.
    else if (Match(TOKEN_VAR))
    {
        VariableDeclaration();
    }
    else
    {
        ExpressionStatement();
    }

    // Condition. We mark the loop-start here.
    int loop_start = CurrentChunk()->count;
    int exit_jump = -1;
    if (!Match(TOKEN_SEMICOLON))
    {
        Expression();
        Consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // If the condition evaluates to false, we jump out of the loop.
        exit_jump = EmitJump(OP_JUMP_IF_FALSE);
        // If not, we pop the evaluated condition of the stack.
        EmitByte(OP_POP);
    }

    // Incrementer.
    // Since the incrementer is parsed before the body, but needs to execute after the body,
    // we use a jump to first jump to the body, then jump back and execute the incrementer.
    if (!Match(TOKEN_RIGHT_PAREN))
    {
        // OP_JUMP to jump to body. Will be patched at the end of the incrementer, which is also
        // the start of the body.
        int body_jump = EmitJump(OP_JUMP);
        // Mark the start of the incrementer so the body can jump back to it.
        int incrementer_start = CurrentChunk()->count;
        // Then parse the incrementer expression.
        Expression();
        // We must remember to pop the expressions value of the stack.
        EmitByte(OP_POP);
        // Validate syntax is correct.
        Consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
        // Add OP_LOOP at end of incrementer body.
        // Note: This is also the end of the for-statement, so technically, this will be executed last.
        EmitLoop(loop_start);
        // We set the loop_start to point to incrementer_start so that when the body
        // emits OP_LOOP, we return to the incrementer, not the top of the loop.
        // This is to achieve what was mentioned above:
        // The incrementer is parsed before the body, so we need to jump to the body then jump back
        // to execute the body first.
        loop_start = incrementer_start;

        // Patch body_jump at end of incrementer/start of body.
        PatchJump(body_jump);
    }

    // Body.
    Statement();

    // Emit OP_LOOP at end of body.
    EmitLoop(loop_start);

    // After loop-body, we backpatch the exit_jump if a conditional is present
    // and emit OP_POP to clear the condition of the stack.
    if (exit_jump != -1)
    {
        PatchJump(exit_jump);
        EmitByte(OP_POP);
    }

    EndScope();
}

void ReturnStatement()
{
    if (current->type == TYPE_SCRIPT)
    {
        Error("Can't return from top-level code.");
    }

    if (Match(TOKEN_SEMICOLON))
    {
        EmitReturn();
    }
    else
    {
        Expression();
        Consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        EmitByte(OP_RETURN);
    }
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

void And_(bool can_assign)
{
    // Left hand side of logical-and is already compiled here, and it's evaluated value will
    // be on top of the stack. We simply emit OP_JUMP_IF_FALSE.
    // If the evaluated value of the left-side is false, we know the entire logical-and is false,
    // so we skip the right-side leaving the evaluated value of the left-side on top of the stack.
    // If the evaluated value is true, we emit OP_POP to pop it off the stack, and we evaluate
    // the right-side.
    int end_jump = EmitJump(OP_JUMP_IF_FALSE);
    EmitByte(OP_POP);
    ParsePrecedence(PREC_AND);
    PatchJump(end_jump);
}

void Or_(bool can_assign)
{
    // Left-hand side is already evaluated here. If it's value is true, we don't need to
    // evaluate the right-hand side, so we hit the OP_JUMP and skip the rest leaving the left-hand
    // side value on the stack. If it's false, we jump to the right-hand side, we hit the OP_POP to pop
    // the evaluated value of the left-hand side of the stack, and we evaluate the right-hand side
    // leaving it's value on top of the stack.
    int else_jump = EmitJump(OP_JUMP_IF_FALSE);
    int end_jump = EmitJump(OP_JUMP);

    // Backpatch else_jump so we jump here if the left-hand side is false, and the emit OP_POP
    // so the evaluated left-side value is popped of the stack.
    PatchJump(else_jump);
    EmitByte(OP_POP);
    ParsePrecedence(PREC_OR);

    // After the parsed right-hand side, we backpatch the end-jump so we offset the IP
    // by the correct amount if the left-hand side is evaluated to true.
    // We don't emit an OP_POP here, as either we reach this point with left-hand side being true,
    // in which case we wish to leave it's value on the stack, or we reach it after processing
    // the right-hand side, in which case we also want to leave it on the stack.
    PatchJump(end_jump);
}

void Call(bool can_assign)
{
    uint8_t arg_count = ArgumentList();
    EmitBytes(OP_CALL, arg_count);
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
    EmitByte(OP_NIL);
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

    if (jump > UINT16_MAX)
    {
        Error("Max offset length of jump-instruction exceeded");
    }

    // Sets the jump-offset so that it points to the instruction following the then-statement.
    CurrentChunk()->code[offset] = (jump >> 8) & 0xFF;
    CurrentChunk()->code[offset + 1] = jump & 0xFF;
}

void EmitLoop(int loop_start)
{
    EmitByte(OP_LOOP);

    // Calculate the offset to the start of the loop and add 2 to account for operands of OP_LOOP.
    int offset = CurrentChunk()->count - loop_start + 2;
    if (offset > UINT16_MAX)
    {
        Error("Size of loop-body exceeds max range of OP_LOOP.");
    }

    // Emit 16-bit offset value in two bytes.
    EmitByte((offset >> 8) & 0xFF);
    EmitByte(offset & 0xFF);
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
    return &current->function->chunk;
}

ObjFunction *EndCompiler()
{
    EmitReturn();

    ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error)
    {
        lox_DisassembleChunk(CurrentChunk(), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
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
    [TOKEN_LEFT_PAREN] = {Grouping, Call, PREC_CALL},
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
    [TOKEN_AND] = {NULL, And_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {Literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {Literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, Or_, PREC_OR},
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

void MarkInitialized()
{
    if (current->scope_depth == 0)
        return;
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

void Function(FunctionType type)
{
    Compiler compiler;
    InitCompiler(&compiler, type);
    BeginScope();

    Consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!Check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            current->function->arity++;
            if (current->function->arity > 255)
            {
                ErrorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = ParseVariable("Expect parameter name.");
            DefineVariable(constant);
        } while (Match(TOKEN_COMMA));
    }
    Consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    Consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    Block();

    ObjFunction *function = EndCompiler();
    EmitBytes(OP_CLOSURE, MakeConstant(OBJ_VAL(function)));
}

uint8_t ArgumentList()
{
    uint8_t arg_count = 0;
    if (!Check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            Expression();
            if (arg_count == 255)
            {
                Error("Can't have more than 255 arguments.");
            }
            arg_count++;
        } while (Match(TOKEN_COMMA));
    }
    Consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return arg_count;
}
