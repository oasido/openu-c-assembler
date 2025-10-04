#ifndef HELPERS_H

#include "types.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

/* arbitrary addition to filename extension */
#define EXT_LENGTH 5

/* for base4 conversion */
#define BITS_PER_BASE4_DIGIT 2  /* 1 base4 digit = 2 bits */
#define BASE4_DIGITS_PER_WORD 5 /* 10 bits / 2 bits per digit = 5 digits */
#define BASE4_STRING_LENGTH 6   /* 5 digits + null terminator = 6 */

/* safe_calloc -- allocate and zero memory
 *
 * - MUST BE FREED!
 *
 * returns allocated memory or NULL on failure
 */
void *safe_calloc(size_t num, size_t size);

/* safe_strdup -- duplicate a string
 *
 * - MUST BE FREED!
 *
 * returns allocated string copy or NULL on failure
 */
char *safe_strdup(const char *s);

/* decimal_to_binary -- produce a 'bits'-wide two's-complement bit string
 * for 'value' using ~(abs(value)) + 1 for negatives
 *
 * - MUST BE FREED!
 *
 * returns a calloc'd null-terminated string of '0'/'1'.
 */
char *decimal_to_binary(int value, int bits);

/* close_files -- close a number of files, remember to add NULL to the end */
/* usage: close_files(f1, f2, NULL) */
int close_files(FILE *, ...);

/* open_file_with_ext -- open a given filename with its extension, return
 * pointer or NULL */
FILE *open_file_with_ext(const char *base, const char *ext, const char *mode);

/* remove_comment -- remove comment lines by inserting an \0 where ';' is
 * found, truncating the text
 */
void remove_comment(char *line);

/* trim_left -- trim leading whitespace from a string
 */
char *trim_left(char *str);

/* trim_right -- trim trailing whitespace from a string
 */
char *trim_right(char *str);

/* trim_inbetween -- trim any whitespace "inbetween" any text, while
 * attempting to keep a nice format (e.g. a space after ':' labels, ",", and
 * ignore any spaces inbetween `"` symbols - so strings won't get trimmed)
 */
char *trim_inbetween(char *str);

/* trim -- wrapper function for complete string trimming
 */
char *trim(char *str);

/* cleanup_line -- wrapper function for both removing comments & trimming
 */
char *cleanup_line(char *line);

/* cleanup_file -- wrapper function for removing comments & trimming a file
 */
FILE *cleanup_file(FILE *in, FILE *out);

void check_trailing_comma(char *s, int line_number, int *error_count);

/* is_illegal_name -- used to check validity of names, e.g. returns error if a
 * saved keyword is found, for example in macros & labels
 */
bool is_illegal_name(char *name);

/* is_valid_data_num -- used directly to check if a string contains only
 * digits, or +/- symbols (which are valid in .data directives) */
bool is_valid_data_num(const char *str);

/* is_num_within_range -- returns true if num is within the allowed 10-bit word
 * range
 */
bool is_num_within_range(short num);

/* decimal_to_base4_letters -- convert decimal number directly to base 4 letters
 *
 * - MUST BE FREED!
 *
 * takes a 10-bit decimal value and returns 5 base-4 letters (a,b,c,d)
 * returns calloc'd string, or NULL on error
 */
char *decimal_to_base4_letters(int decimal_value);

#endif
