#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#define MACRO_START_DIRECTIVE "mcro"  /* start of a macro */
#define MACRO_END_DIRECTIVE "mcroend" /* end of a macro */

/* these are arbitrary values */
#define GROW_BY 256

/* macro -- this struct holds info about a macro: its name, body, line number,
 * and pointer to the next macro */
typedef struct Macro {
  char *name;
  char *body;
  int line_number;
  struct Macro *next;
} Macro;

/* preprocess_file -- runs the preprocessing step for a file (without .as
   extension) cleans up the file, removes comments, finds macros and expands
   them

   returns 0 if ok, 1 if error. consider returning true/false (and inverting)
   */
int preprocess_file(char *filename_without_extension);

#endif /* PREPROCESSOR_H */
