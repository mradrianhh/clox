#ifndef _CLOX_DEBUG_H_
#define _CLOX_DEBUG_H_

#include "core/chunk.h"

void lox_DisassembleChunk(Chunk *chunk, const char *name);
int lox_DisassembleInstruction(Chunk *chunk, int offset);

#endif
