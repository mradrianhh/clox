#include <stdio.h>

#include "compiler/compiler.h"
#include "compiler/scanner.h"

void lox_Compile(const char *source)
{
    lox_InitScanner(source);

    int line = -1;
    for (;;)
    {
        Token token = lox_ScanToken();
        if (token.line != line)
        {
            printf("%4d ", token.line);
            line = token.line;
        }
        else
        {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF)
            break;
    }
}
