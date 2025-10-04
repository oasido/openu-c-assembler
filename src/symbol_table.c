#include "symbol_table.h"
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Symbol *add_symbol(Symbol **head, char *name, int address, SymbolType type) {
  size_t name_len;
  const char *RESERVED_WORDS[] = {
      "mov",  "cmp",    "add", "sub",    "not",   "clr", "lea", "inc",
      "dec",  "jmp",    "bne", "red",    "prn",   "jsr", "rts", "stop",
      "data", "string", "mat", "extern", "entry", "r0",  "r1",  "r2",
      "r3",   "r4",     "r5",  "r6",     "r7",    NULL};
  int idx;

  Symbol *sym;

  /* to be used for checking duplicates */
  Symbol *existing;

  if (!head || !name)
    return NULL;

  /* validate name length is within allowed limits */
  name_len = strlen(name);
  if (name_len == 0 || name_len > MAX_LABEL_LENGTH) {
    fprintf(stderr,
            "(ERROR) [symbol] symbol named '%s' is too long!\nlength must be "
            "between 1-%d chars\n",
            name, MAX_LABEL_LENGTH);
    return NULL;
  }

  /* reject reserved words like opcodes and register names */
  for (idx = 0; RESERVED_WORDS[idx] != NULL; ++idx) {
    if (strcmp(name, RESERVED_WORDS[idx]) == 0) {
      fprintf(stderr,
              "(ERROR) [symbol] '%s' is a reserved word and must be "
              "changed\n",
              name);
      return NULL;
    }
  }

  /* NOTE: macro name conflicts not checked here because macros are expanded
   * during preprocessing before symbol table creation */

  /* prevent duplicate symbol definitions */
  existing = find_symbol(*head, name);
  if (existing) {

    /* handle different types of symbol conflicts */
    if (existing->type == SYMBOL_EXTERNAL) {
      if (type != SYMBOL_EXTERNAL) {
        /* the symbol was declared extern before, now being defined in
         * this file */
        fprintf(stderr,
                "(ERROR) [symbol] symbol named '%s' was declared as "
                "external and can't be defined internally\n",
                existing->name);
      } else {
        /* duplicate .extern declaration */
        fprintf(stderr,
                "(ERROR) [symbol] duplicate extern decleration for symbol "
                "'%s' found\n",
                name);
      }
    } else {
      /* symbol was already defined as code or data in this file */
      fprintf(stderr,
              "(ERROR) [symbol] duplicate symbol decleration for symbol "
              "'%s' found\n",
              name);
    }
    return NULL;
  }

  /* create new symbol node and initialize fields */
  sym = safe_calloc(1, sizeof(Symbol));

  /* copy name safely with null termination guarantee */
  strncpy(sym->name, name, MAX_LABEL_LENGTH);
  sym->name[MAX_LABEL_LENGTH - 1] = '\0';

  /* set symbol properties */
  sym->address = address;
  sym->type = type;
  sym->next = NULL;

  /* add to beginning of linked list (faster & no need for it to be last) */
  sym->next = *head;
  *head = sym;

  return sym;
}

Symbol *find_symbol(Symbol *head, char *name) {
  Symbol *current;

  if (!name)
    return NULL;

  /* walk the linked list looking for exact name match */
  current = head;

  while (current) {
    if (strcmp(current->name, name) == 0) {
      return current;
    }
    current = current->next;
  }

  return NULL;
}
