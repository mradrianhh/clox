#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "vm/vm.h"
#include "core/debug.h"
#include "core/value.h"
#include "compiler/compiler.h"
#include "core/memory.h"
#include "core/object.h"

VM vm;

static InterpretResult Run();
static void ResetStack();
static Value Peek(int distance);
static bool IsFalsey(Value value);
static void Concatenate();
static void RuntimeError(const char *format, ...);

void lox_InitVM()
{
    ResetStack();
    vm.objects = NULL;
}

void lox_FreeVM()
{
    lox_FreeObjects();
}

InterpretResult lox_InterpretChunk(Chunk *chunk)
{
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return Run();
}

InterpretResult lox_InterpretSource(const char *source)
{
    Chunk chunk;
    lox_InitChunk(&chunk);

    if (!lox_Compile(source, &chunk))
    {
        lox_FreeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;
    InterpretResult result = Run();

    lox_FreeChunk(&chunk);
    return result;
}

void lox_PushStack(Value value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

Value lox_PopStack()
{
    vm.stack_top--;
    return *vm.stack_top;
}

InterpretResult Run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(value_type, op)                       \
    do                                                  \
    {                                                   \
        if (!IS_NUMBER(Peek(0)) || !IS_NUMBER(Peek(1))) \
        {                                               \
            RuntimeError("Operands must be numbers.");  \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        double b = AS_NUMBER(lox_PopStack());           \
        double a = AS_NUMBER(lox_PopStack());           \
        lox_PushStack(value_type(a op b));              \
    } while (false)

    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm.stack; slot < vm.stack_top; slot++)
        {
            printf("[ ");
            lox_PrintValue(*slot);
            printf(" ]");
        }
        printf("\n");
        lox_DisassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction = READ_BYTE();
        switch (instruction)
        {
        case OP_CONSTANT:
            Value constant = READ_CONSTANT();
            lox_PushStack(constant);
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(Peek(0)))
            {
                RuntimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            lox_PushStack(NUMBER_VAL(-AS_NUMBER(lox_PopStack())));
            break;
        case OP_ADD:
            if (IS_STRING(Peek(0)) && IS_STRING(Peek(1)))
            {
                Concatenate();
            }
            else if (IS_NUMBER(Peek(0)) && IS_NUMBER(Peek(1)))
            {
                double b = AS_NUMBER(lox_PopStack());
                double a = AS_NUMBER(lox_PopStack());
                lox_PushStack(NUMBER_VAL(a + b));
            }
            else
            {
                RuntimeError("Operands must be two numbers or two strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        case OP_SUBTRACT:
            BINARY_OP(NUMBER_VAL, -);
            break;
        case OP_MULTIPLY:
            BINARY_OP(NUMBER_VAL, *);
            break;
        case OP_DIVIDE:
            BINARY_OP(NUMBER_VAL, /);
            break;
        case OP_NIL:
            lox_PushStack(NIL_VAL);
            break;
        case OP_TRUE:
            lox_PushStack(BOOL_VAL(true));
            break;
        case OP_FALSE:
            lox_PushStack(BOOL_VAL(false));
            break;
        case OP_NOT:
            lox_PushStack(BOOL_VAL(IsFalsey(lox_PopStack())));
            break;
        case OP_EQUAL:
        {
            Value b = lox_PopStack();
            Value a = lox_PopStack();
            lox_PushStack(BOOL_VAL(lox_ValuesEqual(a, b)));
            break;
        }
        case OP_GREATER:
            BINARY_OP(BOOL_VAL, >);
            break;
        case OP_LESS:
            BINARY_OP(BOOL_VAL, <);
            break;
        case OP_RETURN:
            lox_PrintValue(lox_PopStack());
            printf("\n");
            return INTERPRET_OK;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

void ResetStack()
{
    vm.stack_top = vm.stack;
}

Value Peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

bool IsFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void Concatenate()
{
    ObjString *b = AS_STRING(lox_PopStack());
    ObjString *a = AS_STRING(lox_PopStack());

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString *result = lox_TakeString(chars, length);
    lox_PushStack(OBJ_VAL(result));
}

void RuntimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    ResetStack();
}
