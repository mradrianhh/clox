#include <stdio.h>

#include "core/debug.h"
#include "core/value.h"

static int SimpleInstruction(const char *name, int offset);
static int ConstantInstruction(const char *name, Chunk *chunk, int offset);
static int ByteInstruction(const char *name, Chunk *chunk, int offset);
static int JumpInstruction(const char *name, int sign, Chunk *chunk, int offset);

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
    case OP_PRINT:
        return SimpleInstruction("OP_PRINT", offset);
    case OP_POP:
        return SimpleInstruction("OP_POP", offset);
    case OP_DEFINE_GLOBAL:
        return ConstantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_GET_GLOBAL:
        return ConstantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
        return ConstantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_LOCAL:
        return ByteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
        return ByteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_JUMP:
        return JumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
        return JumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP:
        return JumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
        return ByteInstruction("OP_CALL", chunk, offset);
    case OP_CLOSURE:
    {
        offset++;
        uint8_t constant = chunk->code[offset++];
        printf("%-16s %4d ", "OP_CLOSURE", constant);
        lox_PrintValue(chunk->constants.values[constant]);
        printf("\n");
        return offset;
    }
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

int ByteInstruction(const char *name, Chunk *chunk, int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

int JumpInstruction(const char *name, int sign, Chunk *chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset,
           offset + 3 + sign * jump);
    return offset + 3;
}
