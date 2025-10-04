#ifndef TYPES_H
#define TYPES_H

/* ======================================================================= */
/* ============================== global ================================= */
/* ======================================================================= */

/* fake boolean */
typedef int bool;
#define true 1
#define false 0

/* ======================================================================= */
/* ========================== "cross-module" types ======================= */
/* ======================================================================= */

typedef struct DirectiveFields {
  char *label;
  short *data;
  int data_length;
  int data_address; /* dc address */
  char *arg_label;  /* for .entry and .extern, e.g. .entry arg_label */
  bool is_extern;
  bool is_entry;
} DirectiveFields;

typedef struct CommandFields {
  char *label;
  int cmd_address; /* ic address */
  int length;      /* total machine words (L) for this instruction */
  int opcode;
  char *src;
  char *dst;
} CommandFields;

#endif /* TYPES_H */
