# CLOX

## Building

Clone and run "make help".

## Project structure

Building and installation is supported by CMake. A separate Makefile is provided to simplify the building process through automated commands.

CloxCore contains the lexical scanner, the parser and the VM.

Source and header files are separated into the two mirrored folder structures "include" and "src".

- Compiler-directory contains all the logic that pertains to the scanner and the parser/compiler. That is, everything that is responsible for converting source code to IR is contained within this folder.
- VM-directory contains the virtual machine that executes the IR.
- Common-directory contains global helpers and structures that have no direct ties to the project, but exists as auxiliary tools.
- Core-directory contains shared logic between all parts of the code.

## Code standard

- Variables follow the snake-case convention. I.e 'local_variable'.
- Structures and functions follow the pascal-case convention. I.e 'PrintSomething()' or 'ObjectLiteral'.
- Non-static functions have the prefix 'lox_'.
- Static functions are declared at the top, and then defined below. Definitions should follow the order of declarations.
- Static variables are declared at the top, after define's and include's.
- Unless a static function is specific to a non-static function, it's definition should be below the definitions of all non-static functions. The structure should be "static declarations"->"non-static definitions"->"static definitions".
