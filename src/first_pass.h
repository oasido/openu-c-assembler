#ifndef FIRST_PASS_H
#define FIRST_PASS_H

#include "symbol_table.h"
#include <stdio.h>

#define IC_INIT_VALUE 100
#define DC_INIT_VALUE 0

/* first_pass -- main function for first scan of assembler input
   sanitizes lines, extracts labels, handles directives, counts instructions
   returns 0 if no errors, 1 if errors, -1 if invalid input */
int first_pass(FILE *input_file, Symbol **sym_table, int *icf, int *dcf);

#endif /* FIRST_PASS_H */
