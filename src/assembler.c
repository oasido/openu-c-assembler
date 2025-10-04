/*
 * Assembler Project (MM14)
 * Author: Ofek Asido
 * https://github.com/oasido
 */

#include "assembler.h"
#include "data_image.h"
#include "first_pass.h"
#include "helpers.h"
#include "instruction_image.h"
#include "preprocessor.h"
#include "second_pass.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>

/* main -- assembler's main function
 *
 * for each input file, runs preprocessing to expand macros,
 * then first pass to build symbol table and parse instructions,
 * then second pass to resolve symbols & generate output files
 */
int main(int argc, char **argv) {
  int idx = 0;
  Symbol *symtab = NULL;
  int icf, dcf;

  /* for first pass */
  FILE *am_file;

  /* if no parameters were passed */
  if (argc < 2) {
    fprintf(stderr, "(ERROR) [assembler] usage: %s [filename-1]...\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* iterate over each filename passed to us as arguements */
  for (idx = 1; idx < argc; ++idx) {
    printf("=== PREPROCESSING STAGE ===\n");
    printf("Input:  %s.as\n", argv[idx]);
    printf("Output: %s.am\n", argv[idx]);
    printf("Expanding macros...\n");

    if (preprocess_file(argv[idx]) != 0) {
      /* log error & skip file */
      fprintf(stderr,
              "(ERROR) [assembler] failed the preprocessing stage for '%s'\n",
              argv[idx]);
      continue;
    }
    printf("Preprocessing completed successfully!\n");

    am_file = open_file_with_ext(argv[idx], ".am", "r");
    if (!am_file) {
      fprintf(stderr, "(ERROR) [assembler] failed to open '%s.am'\n",
              argv[idx]);

      /* skip on error */
      continue;
    }

    /* first pass */
    printf("\n=== FIRST PASS - SYMBOL TABLE CONSTRUCTION ===\n");
    printf("Processing: %s.am\n", argv[idx]);
    printf("Building symbol table and analyzing instructions...\n");
    if (first_pass(am_file, &symtab, &icf, &dcf)) {
      fprintf(stderr, "(ERROR) [assembler] first_pass failed for '%s.am'\n",
              argv[idx]);
      fclose(am_file);
      free_directives();
      continue;
    }
    printf("First pass completed! IC=%d, DC=%d\n", icf, dcf);

    /* check memory overflow */
    if (icf + dcf > MAX_WORDS_MEMORY) {
      fprintf(stderr,
              "(ERROR) [assembler] memory overflow: program requires %d words "
              "but maximum is %d words\n",
              icf + dcf, MAX_WORDS_MEMORY);
      fclose(am_file);
      free_directives();
      continue;
    }

    /* close am_file, we're done reading it */
    fclose(am_file);

    /* second_pass */
    printf("\n=== SECOND PASS - CODE GENERATION ===\n");
    printf("Processing: %s.am\n", argv[idx]);
    printf("Resolving symbols and generating output files...\n");
    if (second_pass(symtab, icf, argv[idx]) != 0) {
      char ob_file[MAX_FILENAME_LENGTH];
      char ent_file[MAX_FILENAME_LENGTH];
      char ext_file[MAX_FILENAME_LENGTH];

      fprintf(stderr, "(ERROR) [assembler] second_pass failed for '%s'\n",
              argv[idx]);

      /* remove any partially generated output files on error */
      sprintf(ob_file, "%s.ob", argv[idx]);
      sprintf(ent_file, "%s.ent", argv[idx]);
      sprintf(ext_file, "%s.ext", argv[idx]);
      remove(ob_file);
      remove(ent_file);
      remove(ext_file);
    } else {
      FILE *check_file;
      printf("Second pass completed successfully!\n");
      printf("Generated files:\n");
      printf("  - %s.ob (object file)\n", argv[idx]);

      /* check if .ent file was generated */
      check_file = open_file_with_ext(argv[idx], ".ent", "r");
      if (check_file) {
        printf("  - %s.ent (entry symbols)\n", argv[idx]);
        fclose(check_file);
      }

      /* check if .ext file was generated */
      check_file = open_file_with_ext(argv[idx], ".ext", "r");
      if (check_file) {
        printf("  - %s.ext (external references)\n", argv[idx]);
        fclose(check_file);
      }

      printf("Assembly complete for %s!\n\n", argv[idx]);
    }

    /* cleanup directives and commands */
    free_directives();
    free_commands();
  } /* end loop */

  return EXIT_SUCCESS;
}
