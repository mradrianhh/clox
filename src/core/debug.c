#include <stdio.h>

#include "core/debug.h"
#include "core/value.h"

static int SimpleInstruction(const char *name, int offset);
static int ConstantInstruction(const char *name, Chunk *chunk, int offset);

void lox_DisassembleChunk(Chunk *chunk, const char *name)
{
    printf("== %s ==\n", name);
    for (size_t offset = 0; offset < chunk->count;)
    {
        offset = lox_DisassembleInstruction(chunk, offset);
    }
}

int lox_DisassembleInstruction(Chunk *chunk, int offset)
{
    printf("%04d ", offset);
    if (offset > 0 &&
        chunk->lines[offset] == chunk->lines[offset - 1])
    {
        printf("   | ");
    }
    else
    {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
    case OP_RETURN:
        return SimpleInstruction("OP_RETURN", offset);
    case OP_CONSTANT:
        return ConstantInstruction("OP_CONSTANT", chunk, offset);
    case OP_NEGATE:
        return SimpleInstruction("OP_NEGATE", offset);
    case OP_ADD:
        return SimpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
        return SimpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
        return SimpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
        return SimpleInstruction("OP_DIVIDE", offset);
    case OP_NIL:
        return SimpleInstruction("OP_NIL", offset);
    case OP_TRUE:
        return SimpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
        return SimpleInstruction("OP_FALSE", offset);
    case OP_NOT:
        return SimpleInstruction("OP_NOT", offset);
    case OP_EQUAL:
        return SimpleInstruction("OP_EQUAL", offset);
    case OP_GREATER:
        return SimpleInstruction("OP_GREATER", offset);
    case OP_LESS:
        return SimpleInstruction("OP_LESS", offset);
    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;
    }
    return 0;
}

int SimpleInstruction(const char *name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

int ConstantInstruction(const char *name, Chunk *chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    lox_PrintValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}
