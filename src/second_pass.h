#ifndef SECOND_PASS_H

#include "symbol_table.h"

/* address/encoding defines for second pass */
#define ADDRESS_PAYLOAD_SHIFT 2

#define ARE_ABSOLUTE 0
#define ARE_EXTERNAL 1
#define ARE_RELOCATABLE 2

/* second_pass -- updates all missing symbol addresses, writes the object file
 * (.ob) and if needed - the entries (.ent) & externals (.ext)
 *
 * we use global command_list, command_count, directive_list, directive_count
 *
 * returns the number of errors found, or -1 if invalid
 */
int second_pass(Symbol *symtab, int icf, const char *base_filename);

#endif /* SECOND_PASS_H */
