/*
 * Assembler Project (MM14)
 * Author: Ofek Asido
 * https://github.com/oasido
 */

#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#define MAX_LABEL_LENGTH 31
#define MAX_SYMBOL_LENGTH 31
#define MAX_LINE_LENGTH 81
#define MAX_FILENAME_LENGTH 100
#define MAX_WORDS_MEMORY 256
#define WORD_SIZE 10

/* we use two's-complement with 10 bits (1 word), so:
 * min = -2^(n-1)=-512,  max = 2^(n-1) - 1=511 */
#define MIN_WORD_VAL (-512)
#define MAX_WORD_VAL 511

/* immediate values use 8-bit signed range (only 8 bits available in instruction
 * word)
 * min = -2^(8-1)=-128,  max = 2^(8-1) - 1=127 */
#define MIN_IMMEDIATE_VAL (-128)
#define MAX_IMMEDIATE_VAL 127

#endif /* !ASSEMBLER_H */
