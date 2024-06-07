#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
#include "core/chunk.h"
#include "core/debug.h"
#include "vm/vm.h"
#include "common/string_helper.h"

static int Run(const char *source);
static int RunFile(const char *path);
static int RunInteractively();
static void ResetTerminal();
static void DisplayHelp();
static void DisplayHeader();
static void TryParseConsoleCommand(const char *input);
static int TryParseFileCommand(const char *input);
static char *GetFileName(const char *input);

int main(int argc, const char **argv)
{
    lox_InitVM();

    if (argc == 1)
    {
        RunInteractively();
    }
    else if (argc == 2)
    {
        RunFile(argv[1]);
    }
    else
    {
        fprintf(stderr, "Usage: lox [path]\n");
        exit(64);
    }

    lox_FreeVM();
    return 0;
}

int Run(const char *source)
{
    lox_InterpretSource(source);
    return LOX_EXIT_SUCCESS;
}

int RunFile(const char *path)
{
    FILE *fp;
    if ((fp = fopen(path, "r")) == NULL)
    {
        printf("Error: Can't open file '%s'.\n", path);
        return LOX_EXIT_FAILURE;
    }

    // find size.
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // read file into char-buffer.
    // Add 1 to allow null-terminating character.
    char fb[size + 1];
    memset(fb, 0, size + 1);
    fread(fb, sizeof(fb), 1, fp);

    // interpret file.
    Run(fb);

    fclose(fp);
    return LOX_EXIT_SUCCESS;
}

int RunInteractively()
{
    char *input = calloc(1, 1), buffer[100];
    ResetTerminal();
    while (fgets(buffer, 100, stdin))
    {
        input = realloc(input, strlen(buffer));
        strcpy(input, buffer);
        if (strcmp(input, ".exit\n") == 0)
        {
            system("clear");
            return LOX_EXIT_SUCCESS;
        }
        else if (strcmp(input, ".clear\n") == 0)
        {
            ResetTerminal();
        }
        else if (strcmp(input, ".help\n") == 0)
        {
            DisplayHelp();
        }
        else if (input[0] == '.')
        {
            TryParseConsoleCommand(input);
        }
        else
        {
            Run(input);
            printf("> ");
        }
    }
    return LOX_EXIT_SUCCESS;
}

void ResetTerminal()
{
    DisplayHeader();
    printf("> ");
}

void DisplayHelp()
{
    DisplayHeader();
    printf("Help page\n\n");
    printf("All commands must be prefixed with '.' to mark them as non-executable code.\n\n");
    printf("List of commands: \n");
    printf("[.exit]             - Terminate the session.\n");
    printf("[.help]             - Display this page.\n");
    printf("[.clear]            - Clear the terminal screen.\n");
    printf("[.file] [filename]  - Run file.\n");
    printf("\n> ");
}

void DisplayHeader()
{
    system("clear");
    printf("Lox Lang Interpreter\n");
    printf("Enter '.help' to see a list of commands.\n\n");
}

void TryParseConsoleCommand(const char *input)
{
    if (strstr(input, ".file"))
    {
        if (TryParseFileCommand(input))
        {
            printf("Invalid .file command. Type '.help' for a list of commands.\n");
        }
    }
    else
    {
        // User attempted invalid console command.
        printf("Invalid console command. Type '.help' for a list of commands.\n");
    }

    printf("> ");
}

int TryParseFileCommand(const char *input)
{
    const char *filename = GetFileName(input);
    if (filename == NULL)
    {
        return LOX_EXIT_FAILURE;
    }
    RunFile(filename);
    return LOX_EXIT_SUCCESS;
}

char *GetFileName(const char *input)
{
    char test[strlen(input)];
    strcpy(test, input);
    // Start reading after '.file' until not space.
    // Thats the initial position.
    // Store the initial position.
    size_t initial;
    for (initial = 5; input[initial] == ' '; initial++)
        ;

    // Continue scanning until you find ' ' or '\n'.
    // Thats the final position.
    size_t final;
    for (final = initial; input[final] != ' ' && input[final] != '\0' && input[final] != '\n'; final++)
        ;

    // Calculate length.
    size_t length = final - initial;
    if (length <= 0)
    {
        return NULL;
    }

    // Allocate a string of calculated length.
    char *filename;

    // Fetch filename with substring.
    lox_Substring(&filename, input, initial, length);

    return filename;
}
