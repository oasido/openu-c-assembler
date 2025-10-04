#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "assembler.h"

/* symbol_table.h -- linked-list symbol table for labels */

/* symbol types (mutually exclusive) */
typedef enum {
  SYMBOL_CODE,    /* label refers to code (instruction address) */
  SYMBOL_DATA,    /* label refers to data (data address) */
  SYMBOL_EXTERNAL /* label defined in another file (extern) */
} SymbolType;

/* symbol -- one label record in the table */
typedef struct Symbol {
  char name[MAX_SYMBOL_LENGTH];
  int address;
  SymbolType type; /* code/data/external */
  int line_number;
  struct Symbol *next;
} Symbol;

/* add_symbol -- define a new label
 * checks name rules, checks for duplicates, allocates memory for a
 * node, sets address/type, and pushes to the head of the list
 *
 * returns the pointer to the new node on success, NULL on error
 */
Symbol *add_symbol(Symbol **head, char *name, int address, SymbolType type);

/* find_symbol -- exact name lookup
 *
 * returns the symbol ptr or NULL if not found
 */
Symbol *find_symbol(Symbol *head, char *name);

#endif /* SYMBOL_TABLE_H */
