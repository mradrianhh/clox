#include <stdio.h>

#include "vm/vm.h"
#include "core/debug.h"
#include "core/value.h"
#include "compiler/compiler.h"

VM vm;

static InterpretResult Run();
static void ResetStack();

void lox_InitVM()
{
    ResetStack();
}

void lox_FreeVM()
{
}

InterpretResult lox_InterpretChunk(Chunk *chunk)
{
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return Run();
}

InterpretResult lox_InterpretSource(const char *source)
{
    lox_Compile(source);
    return INTERPRET_OK;
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
#define BINARY_OP(op) \
    do { \
        double b = lox_PopStack(); \
        double a = lox_PopStack(); \
        lox_PushStack(a op b); \
    } while(false)

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
            lox_PushStack(-lox_PopStack());
            break;
        case OP_ADD:
            BINARY_OP(+);
            break;
        case OP_SUBTRACT:
            BINARY_OP(-);
            break;
        case OP_MULTIPLY:
            BINARY_OP(*);
            break;
        case OP_DIVIDE:
            BINARY_OP(/);
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
