#ifndef DATA_IMAGE_H
#define DATA_IMAGE_H

#include "assembler.h"
#include "types.h"

extern DirectiveFields *directive_list[MAX_WORDS_MEMORY];

extern int directive_count;

/* append_directive -- push a DirectiveFields into the list
 *
 * on overflow it will print error and free df to avoid a leak */
void append_directive(DirectiveFields *df);

/* new_directive -- allocate and initialize a DirectiveFields of given length
 *
 * - MUST BE FREED!
 *
 * returns a pointer to an empty DirectiveFields, or NULL on calloc
 * failure
 */
DirectiveFields *new_directive(int data_length);

/* free_directive -- release one DirectiveFields and its owned memory */
void free_directive(DirectiveFields *instr);

/* free_directives -- reset & free all stored directives
 *
 * iterates directive_list & frees eachone, nulls slots,
 * and sets directive_count back to 0 */
void free_directives();

#endif /* DATA_IMAGE_H */
