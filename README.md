![cover](cover.png)

# assembler

a two-pass assembler implementation in ansi c that processes assembly language files.

## what's included

- project specifications (pdf file in this directory)
- complete assembler source code
- makefile for building

## how it works

the assembler processes .as files through these stages:

**preprocessor** - handles macro definitions and expansions, creates .am files

**first pass** - builds symbol table, assigns addresses to labels, validates syntax

**second pass** - resolves symbol references, generates machine code, produces output files

_happiness 😇 or sadness 😭_

## build and run

```bash
make
./assembler filename1 filename2 ... # files need must have .as format
```

## output files

- .am - trimmed file with macros expanded
- .ob - object file with machine code
- .ent - entry symbols
- .ext - external symbols

## memory layout

- 256 words max memory
- 10-bit words
- instructions start at address 100
- data follows code section

## file structure

key files:

- assembler.c/h - main driver
- preprocessor.c/h - macro handling
- first_pass.c/h - symbol table construction
- second_pass.c/h - code generation
- symbol_table.c/h - symbol management
- instruction_image.c/h - instruction encoding
- data_image.c/h - data directive handling
