#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "vm/vm.h"
#include "core/debug.h"
#include "core/value.h"
#include "compiler/compiler.h"
#include "core/memory.h"
#include "core/object.h"

VM vm;

static Value clockNative(int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static InterpretResult Run();
static void ResetStack();
static Value Peek(int distance);
static bool IsFalsey(Value value);
static void Concatenate();
static void RuntimeError(const char *format, ...);
static bool CallValue(Value callee, int arg_count);
static bool Call(ObjFunction *function, int arg_count);
static void DefineNative(const char *name, NativeFn function);

void lox_InitVM()
{
    ResetStack();
    vm.objects = NULL;
    lox_InitHashTable(&vm.strings);
    lox_InitHashTable(&vm.globals);

    DefineNative("clock", clockNative);
}

void lox_FreeVM()
{
    lox_FreeHashTable(&vm.strings);
    lox_FreeHashTable(&vm.globals);
    lox_FreeObjects();
}

InterpretResult lox_InterpretSource(const char *source)
{
    ObjFunction *function = lox_Compile(source);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    lox_PushStack(OBJ_VAL(function));
    Call(function, 0);
    return Run();
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
    CallFrame *frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, \
     (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
    (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
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
        lox_DisassembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
#endif
        uint8_t instruction = READ_BYTE();
        switch (instruction)
        {
        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT();
            lox_PushStack(constant);
            break;
        }
        case OP_NEGATE:
        {
            if (!IS_NUMBER(Peek(0)))
            {
                RuntimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            lox_PushStack(NUMBER_VAL(-AS_NUMBER(lox_PopStack())));
            break;
        }
        case OP_ADD:
        {
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
        }
        case OP_SUBTRACT:
        {
            BINARY_OP(NUMBER_VAL, -);
            break;
        }
        case OP_MULTIPLY:
        {
            BINARY_OP(NUMBER_VAL, *);
            break;
        }
        case OP_DIVIDE:
        {
            BINARY_OP(NUMBER_VAL, /);
            break;
        }
        case OP_NIL:
        {
            lox_PushStack(NIL_VAL);
            break;
        }
        case OP_TRUE:
        {
            lox_PushStack(BOOL_VAL(true));
            break;
        }
        case OP_FALSE:
        {
            lox_PushStack(BOOL_VAL(false));
            break;
        }
        case OP_NOT:
        {
            lox_PushStack(BOOL_VAL(IsFalsey(lox_PopStack())));
            break;
        }
        case OP_EQUAL:
        {
            Value b = lox_PopStack();
            Value a = lox_PopStack();
            lox_PushStack(BOOL_VAL(lox_ValuesEqual(a, b)));
            break;
        }
        case OP_GREATER:
        {
            BINARY_OP(BOOL_VAL, >);
            break;
        }
        case OP_LESS:
        {
            BINARY_OP(BOOL_VAL, <);
            break;
        }
        case OP_PRINT:
        {
            lox_PrintValue(lox_PopStack());
            printf("\n");
            break;
        }
        case OP_POP:
        {
            lox_PopStack();
            break;
        }
        case OP_DEFINE_GLOBAL:
        {
            ObjString *name = READ_STRING();
            lox_AddEntryHashTable(&vm.globals, name, Peek(0));
            lox_PopStack();
            break;
        }
        case OP_GET_GLOBAL:
        {
            ObjString *name = READ_STRING();
            Value value;
            if (!lox_GetEntryHashTable(&vm.globals, name, &value))
            {
                RuntimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            lox_PushStack(value);
            break;
        }
        case OP_SET_GLOBAL:
        {
            ObjString *name = READ_STRING();
            if (lox_AddEntryHashTable(&vm.globals, name, Peek(0)))
            {
                lox_RemoveEntryHashTable(&vm.globals, name);
                RuntimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_GET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            lox_PushStack(frame->slots[slot]);
            break;
        }
        case OP_SET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = Peek(0);
            break;
        }
        case OP_JUMP_IF_FALSE:
        {
            // Read the offset of the jump instruction and adjust ip if condition is false.
            uint16_t offset = READ_SHORT();
            if (IsFalsey(Peek(0)))
            {
                frame->ip += offset;
            }
            break;
        }
        case OP_JUMP:
        {
            // Jump is unconditional, so we simply increase the IP.
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OP_LOOP:
        {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case OP_CALL:
        {
            int arg_count = READ_BYTE();
            if (!CallValue(Peek(arg_count), arg_count))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frame_count - 1];
            break;
        }
        case OP_RETURN:
        {
            Value result = lox_PopStack();
            vm.frame_count--;
            if (vm.frame_count == 0)
            {
                lox_PopStack();
                return INTERPRET_OK;
            }

            vm.stack_top = frame->slots;
            lox_PushStack(result);
            frame = &vm.frames[vm.frame_count - 1];
            break;
        }
        default:
            break;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef READ_SHORT
#undef BINARY_OP
}

void ResetStack()
{
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
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

    for (int i = vm.frame_count - 1; i >= 0; i--)
    {
        CallFrame *frame = &vm.frames[i];
        ObjFunction *function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ",
                function->chunk.lines[instruction]);
        if (function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    ResetStack();
}

bool CallValue(Value callee, int arg_count)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
        case OBJ_FUNCTION:
            return Call(AS_FUNCTION(callee), arg_count);
        case OBJ_NATIVE:
        {
            NativeFn native = AS_NATIVE(callee);
            Value result = native(arg_count, vm.stack_top - arg_count);
            vm.stack_top -= arg_count + 1;
            lox_PushStack(result);
            return true;
        }
        default:
            break; // Non-callable object type.
        }
    }
    RuntimeError("Can only call functions and classes.");
    return false;
}

bool Call(ObjFunction *function, int arg_count)
{
    if (arg_count != function->arity)
    {
        RuntimeError("Expected %d arguments but got %d.",
                     function->arity, arg_count);
        return false;
    }

    if (vm.frame_count == FRAMES_MAX)
    {
        RuntimeError("Stack overflow.");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frame_count++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm.stack_top - arg_count - 1;
    return true;
}

void DefineNative(const char *name, NativeFn function)
{
    lox_PushStack(OBJ_VAL(lox_CopyString(name, (int)strlen(name))));
    lox_PushStack(OBJ_VAL(lox_CreateNative(function)));
    lox_AddEntryHashTable(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    lox_PopStack();
    lox_PopStack();
}
