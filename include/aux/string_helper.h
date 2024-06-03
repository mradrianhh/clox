#ifndef _CLOX_STRING_HELPER_H_
#define _CLOX_STRING_HELPER_H_

#include "common.h"

void lox_Substring(char **dest, const char *src, size_t start, size_t length);
bool lox_IsDigit(char ch);
bool lox_IsAlpha(char ch);
bool lox_IsAlphaNumeric(char ch);

#endif
