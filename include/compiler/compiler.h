#ifndef _CLOX_COMPILER_H_
#define _CLOX_COMPILER_H_

#include "common.h"
#include "core/chunk.h"

bool lox_Compile(const char *source, Chunk *chunk);

#endif
