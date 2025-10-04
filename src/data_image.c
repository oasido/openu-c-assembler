#include "data_image.h"
#include "assembler.h"
#include "helpers.h"
#include "instruction_image.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>

/* data_image -- collects data directives (.data, .mat, ...)
 * during first pass */

/* pending directives captured during first pass */
DirectiveFields *directive_list[MAX_WORDS_MEMORY];
int directive_count = 0;

void append_directive(DirectiveFields *df) {
  /* check for overflow before adding to list */
  /* this check is not accurate, that's why we have a similar check in
   * assembler.c */
  if (command_count + directive_count >= MAX_WORDS_MEMORY) {
    fprintf(stderr,
            "(ERROR) [data_image] directive list overflow, dropping entry\n");

    /* clean up the directive we cannot store to prevent memory leak */
    free_directive(df);

    return;
  }

  /* add directive to global list and increment counter */
  directive_list[directive_count++] = df;
}

DirectiveFields *new_directive(int data_length) {
  DirectiveFields *df;

  /* allocate main directive structure */
  df = safe_calloc(1, sizeof(*df));
  if (!df)
    return NULL;

  /* set data length and initialize address to 0 (will be set later) */
  df->data_length = data_length;
  df->data_address = 0;

  /* allocate data array if needed (for .data, .string, .mat directives) */
  if (data_length > 0) {
    df->data = safe_calloc((size_t)data_length, sizeof(*df->data));
    if (!df->data) {
      /* cleanup main structure if data allocation failed */
      free(df);
      return NULL;
    }
  }

  /* initialize string pointers to NULL and flags to default values */
  df->label = NULL;
  df->arg_label = NULL;
  df->is_extern = false;
  return df;
}

void free_directive(DirectiveFields *df) {
  if (!df)
    return;

  /* free all allocated strings and data array */
  free(df->label);     /* label string (if any) */
  free(df->arg_label); /* argument label for .extern/.entry (if any) */
  free(df->data);      /* data array (if any) */

  /* free the main structure */
  free(df);
}

void free_directives() {
  int i;

  /* free all directives in the global list */
  for (i = 0; i < directive_count; ++i) {
    free_directive(directive_list[i]); /* free each directive and its data */
    directive_list[i] = NULL;          /* clear pointer for safety */
  }

  /* reset counter to empty state */
  directive_count = 0;
}
