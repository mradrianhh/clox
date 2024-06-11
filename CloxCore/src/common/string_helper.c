#include <stdlib.h>
#include <string.h>

#include "common/string_helper.h"

void lox_Substring(char **dest, const char *src, size_t start, size_t length)
{
    (*dest) = calloc(1, length + 1);
    for (int i = start; i < start + length; i++)
    {
        (*dest)[i - start] = src[i];
    }
    (*dest)[length + 1] = '\0';
}

bool lox_IsDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

bool lox_IsAlpha(char ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           ch == '_';
}

bool lox_IsAlphaNumeric(char ch)
{
    return lox_IsAlpha(ch) || lox_IsDigit(ch);
}
