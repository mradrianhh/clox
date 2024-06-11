#ifndef _CLOX_COMPILER_H_
#define _CLOX_COMPILER_H_

#include "common/common.h"
#include "core/object.h"

ObjFunction *lox_Compile(const char *source);

#endif
