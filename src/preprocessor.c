#include "preprocessor.h"
#include "assembler.h"
#include "helpers.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* preprocessor -- handles macro definitions and expansions in assembly files */

/* basic macro node funcs */
static Macro *macro_create(Macro *head, char *name, char *body,
                           int line_number);
static Macro *macro_find(Macro *head, char *name);
static bool macro_is_already_defined(Macro *head, char *name);
static int macro_push(Macro **head, Macro *macro_node);
static void macro_free(Macro *macro_node);
static void macro_free_all(Macro **head);

/* macro handling funcs */
static bool begin_macro_definition(const char *line, int line_num, Macro **head,
                                   char *macro_name_out, int *start_line_out);
static bool end_macro_definition(const char *macro_name, char **pbody,
                                 size_t *p_len, size_t *p_cap, int start_line,
                                 Macro **head);
static bool expand_macro_or_emit_line(const char *line, char *first_token,
                                      FILE *out, Macro **head, int line_num);
static bool has_extra_after_macro(const char *line);
static int macro_scan(FILE *in, FILE *out, Macro **head);

/* ======================================================================= */
/* ===================== public api -- see header ======================== */
/* ======================================================================= */

int preprocess_file(char *filename_without_extension) {
  FILE *input_file = NULL, *trimmed_file = NULL, *output_file = NULL;

  char input_filename[MAX_FILENAME_LENGTH],
      output_filename[MAX_FILENAME_LENGTH];

  char *in_ext = ".as", *out_ext = ".am";

  Macro *head = NULL;

  /* check if exceeds filename lenght */
  if (strlen(filename_without_extension) + strlen(in_ext) >
      MAX_FILENAME_LENGTH) {
    fprintf(stderr, "(ERROR) [preprocessor] filename too long\n");
    return 1;
  }

  /* build the filenames */
  strcpy(input_filename, filename_without_extension);
  strcat(input_filename, in_ext);

  strcpy(output_filename, filename_without_extension);
  strcat(output_filename, out_ext);

  input_file = open_file_with_ext(filename_without_extension, in_ext, "r");
  if (!input_file) {
    fprintf(stderr, "(ERROR) [preprocessor] opening input file failed\n");
    return 1;
  }

  output_file = open_file_with_ext(filename_without_extension, out_ext, "w");
  if (!output_file) {
    fprintf(stderr, "(ERROR) [preprocessor] creating input file failed\n");
    fclose(input_file);
    return 1;
  }

  /* temporary file to hold cleaned lines */
  trimmed_file = tmpfile();
  if (!trimmed_file) {
    fprintf(stderr,
            "(ERROR) [preprocessor] creating & opening temp file failed\n");
    close_files(input_file, output_file, NULL);
    return 1;
  }

  /* strip comments and spaces into the temp file */
  cleanup_file(input_file, trimmed_file);

  /* stop early if the file ended up empty */
  /* consider simplifying? */
  fseek(trimmed_file, 0, SEEK_END);
  if (ftell(trimmed_file) == 0) {
    printf("(ERROR) [preprocessor] file empty after trimming, returning.\n");
    close_files(input_file, trimmed_file, output_file, NULL);
    return 0;
  }

  /* go to beginning of trimmed file */
  rewind(trimmed_file);

  if (!macro_scan(trimmed_file, output_file, &head)) {
    macro_free_all(&head);
    close_files(input_file, trimmed_file, output_file, NULL);
    return 1;
  }

  macro_free_all(&head);
  close_files(input_file, trimmed_file, output_file, NULL);
  return 0;
}

/* ======================================================================= */
/* ========================== static helpers ============================== */
/* ======================================================================= */

/* macro_create -- build a macro node and fill its fields
 * rejects a duplicate name so we do not shadow an existing macro
 */
static Macro *macro_create(Macro *head, char *name, char *body,
                           int line_number) {
  Macro *macro_node;
  /* reject duplicates before allocating */
  if (macro_is_already_defined(head, name)) {
    /* error reported through the macro_is_already_defined function */
    /* we return NULL */
    return NULL;
  }

  /* allocate the new struct */
  macro_node = safe_calloc(1, sizeof(Macro));
  if (!macro_node)
    return NULL;

  macro_node->name = name;
  macro_node->body = body;
  macro_node->line_number = line_number;
  macro_node->next = NULL;

  return macro_node;
}

/* macro_find -- looks for a macro with the given name in the list. returns
 * pointer if found, null if not. */
static Macro *macro_find(Macro *head, char *name) {
  Macro *temp = head;

  /* iterate through macros and compare by name */
  while (temp != NULL) {
    if (strcmp(name, temp->name) == 0) {
      return temp;
    }
    temp = temp->next;
  }

  return NULL;
}

/* macro_is_already_defined -- check if a macro is already defined, we compare
 * by name
 *
 * returns true if already defined, false if not
 */
static bool macro_is_already_defined(Macro *head, char *name) {
  /* if macro already defined */
  if (macro_find(head, name)) {
    fprintf(stderr,
            "(ERROR) [preprocessor] macro_create: macro '%s' was defined more "
            "than once\n",
            name);
    return true;
  }

  /* macro was not defined */
  return false;
}

/* macro_push -- adds a macro node to the end of the macro list
 *
 * returns 0 if ok, -1 if duplicate */
static int macro_push(Macro **head, Macro *macro_node) {
  Macro *temp;

  /* avoid duplicates */
  if (macro_is_already_defined(*head, macro_node->name)) {
    /* error reported through the macro_is_already_defined function */
    /* we return NULL */
    return -1;
  }

  if (*head == NULL) {
    *head = macro_node;
    macro_node->next = NULL;
    return 0;
  }

  /* walk to the end */
  temp = *head;
  while (temp->next != NULL) {
    temp = temp->next;
  }

  /* add the new macro node to the end of the list */
  temp->next = macro_node;
  macro_node->next = NULL;
  return 0;
}

/* macro_free -- frees memory for a single macro node */
static void macro_free(Macro *macro_node) {
  if (!macro_node)
    return;
  free(macro_node->name);
  free(macro_node->body);
  free(macro_node);
}

/* macro_free_all -- frees all macro nodes in the list */
static void macro_free_all(Macro **head) {
  Macro *temp;

  if (head == NULL || *head == NULL)
    return;

  /* pop each item and free it */
  while (*head) {
    temp = *head;
    *head = (*head)->next;
    macro_free(temp);
  }

  *head = NULL;
}

/* ======================================================================= */

/* begin_macro_definition -- validate header and record name + start line
 * expects: "mcro <name>" and nothing else on the line
 *
 * we check:
 *  - name exists
 *  - no extra tokens
 *  - name is legal
 *  - no duplicate macro
 *
 * returns true on success, false on error
 */
static bool begin_macro_definition(const char *line, int line_num, Macro **head,
                                   char *macro_name_out, int *start_line_out) {
  char tmp[MAX_LINE_LENGTH];
  char *name = NULL;
  char *extra = NULL;

  /* copy and tokenize: get "mcro", then the name, then ensure no extra */
  strcpy(tmp, line);

  strtok(tmp, " \t");          /* "mcro", "skip it" (we don't need it) */
  name = strtok(NULL, " \t");  /* name */
  extra = strtok(NULL, " \t"); /* any extra text after */

  if (!name) {
    fprintf(stderr, "(ERROR) [preprocessor] macro without name definition\n");
    return false;
  }

  if (extra) {
    /* extra text after macro name */
    fprintf(stderr,
            "(ERROR) [preprocessor] extra text after macro name definition\n");
    return false;
  }

  if (is_illegal_name(name)) {
    fprintf(stderr, "(ERROR) [preprocessor] illegal name '%s' for a macro\n",
            name);
    return false;
  }

  if (macro_is_already_defined(*head, name)) {
    /* we already print to stderr in macro_is_already_defined */
    return false;
  }

  /* record name and start line for the body of the macro "recording" phase */
  strcpy(macro_name_out, name);
  *start_line_out = line_num;

  return true;
}

/* end_macro_definition -- create and push the macro object, then reset body
 *
 * returns true on success, false on error
 */
static bool end_macro_definition(const char *macro_name, char **pbody,
                                 size_t *p_len, size_t *p_cap, int start_line,
                                 Macro **head) {
  char *name_copy = NULL;
  char *body_copy = NULL;
  Macro *m = NULL;

  /* duplicate name and body */
  name_copy = safe_strdup(macro_name);
  body_copy = safe_strdup((*pbody) ? (*pbody) : "");

  /* if malloc failed, error was reported in safe_strdup, we free and return
   * false */
  if (!name_copy || !body_copy) {
    free(name_copy);
    free(body_copy);
    free(*pbody);
    *pbody = NULL;
    *p_len = 0;
    *p_cap = 0;
    return false;
  }

  /* create and push the macro */
  m = macro_create(*head, name_copy, body_copy, start_line);

  /* if macro_create returned NULL - i.e.it failed, we free and return */
  if (!m) {
    free(name_copy);
    free(body_copy);
    free(*pbody);
    *pbody = NULL;
    *p_len = 0;
    *p_cap = 0;
    return false;
  }

  macro_push(head, m);

  /* after use we free the buffers */
  free(*pbody);
  *pbody = NULL;
  *p_len = 0;
  *p_cap = 0;

  return true;
}

/* expand_macro_or_emit_line -- if first token is a known macro, expand it
 * otherwise write the original line to out
 *
 * returns true on success, false on error
 *
 * we're checking:
 *  - no extra text after the macro call
 *  - if macro is used before its declared
 */
static bool expand_macro_or_emit_line(const char *line, char *first_token,
                                      FILE *out, Macro **head, int line_num) {
  Macro *m = macro_find(*head, first_token);

  if (m) {
    /* check that there is no extra token after the macro call name */
    if (has_extra_after_macro(line)) {
      fprintf(
          stderr,
          "(ERROR) [preprocessor] Macros expansion in an .as file failed\n");
      return false;
    }

    /* macro must be declared before it is used */
    if (m->line_number > line_num) {
      fprintf(stderr, "(ERROR) [preprocessor] Macro call before declaration\n");
      return false;
    }

    /* write the macro body to the output stream */
    /* no need for fprintf because we copy it "as is", newline is NOT needed! */
    fputs(m->body, out);

  } else {
    /* check if first token is a label and second token is a macro */
    int token_len = strlen(first_token);
    if (token_len > 1 && first_token[token_len - 1] == ':') {
      char tmp[MAX_LINE_LENGTH];
      char *second_token;

      strcpy(tmp, line);
      strtok(tmp, " \t");                 /* skip label */
      second_token = strtok(NULL, " \t"); /* get second token */

      if (second_token) {
        Macro *macro = macro_find(*head, second_token);
        if (macro) {
          /* ensure no extra text after label + macro */
          char *third_token = strtok(NULL, " \t");
          if (third_token) {
            fprintf(stderr,
                    "(ERROR) [preprocessor] Extra text after macro call\n");
            return false;
          }

          /* check macro declared before use */
          if (macro->line_number > line_num) {
            fprintf(stderr,
                    "(ERROR) [preprocessor] Macro call before declaration\n");
            return false;
          }

          /* write label followed by macro body */
          fprintf(out, "%s ", first_token);
          fputs(macro->body, out);
          return true;
        }
      }
    }

    /* NOT a macro call - copy the line and add newline */
    fprintf(out, "%s\n", line);
  }
  return true;
}

/* has_extra_after_macro -- returns true if a macro name has text after it
 *
 * note: it will work for any text (not just a macro) */
static bool has_extra_after_macro(const char *line) {
  char tmp[MAX_LINE_LENGTH];
  char *t = NULL;

  strcpy(tmp, line);
  t = strtok(tmp, " \t");  /* mcro token */
  t = strtok(NULL, " \t"); /* second token/extra, if any */

  /* returns true if macro (or any text really) has extra text after it */
  return t != NULL;
}

/* append_body_line -- append the given line plus a newline to the growing body
 * returns true on success, false on failure
 */
static bool append_body_line(char **pbody, size_t *p_len, size_t *p_cap,
                             const char *line) {

  int line_length = strlen(line);

  size_t bytes_needed = *p_len + line_length + 2;
  /* +2 for '\n' and '\0' */

  if (!pbody || !p_len || !p_cap || !line) {
    fprintf(stderr, "(ERROR) [preprocessor] Missing one or more params in "
                    "append_body_line\n");
    return false;
  }

  /* if we need more space, wegrow buffer by GROW_BY bytes */
  if (bytes_needed > *p_cap) {
    int new_capacity = bytes_needed + GROW_BY;

    char *new_buffer = realloc(*pbody, new_capacity);

    if (new_buffer == NULL) {
      /* realloc failed */
      return false;
    }

    /* update */
    *pbody = new_buffer;
    *p_cap = new_capacity;
  }

  /* copy line to body */
  strcpy((*pbody) + *p_len, line);

  /* update length */
  *p_len += line_length;

  /* add newline */
  (*pbody)[*p_len] = '\n';

  /* update length again (because of newline) */
  (*p_len)++;

  /* make sure the body is null terminated */
  (*pbody)[*p_len] = '\0';

  /* success */
  return true;
}

/* macro_scan -- reads macros from input file, expands them, and writes to
 * output file
 *
 *  - we read each line:
 *    - if inside a macro, we either add lines from it or end it
 *    - if we're outside a macro, we either start defining a new macro, start
 *      expanding a new macro, or copy the line as is
 *
 *  - at EOF wee fail if macro is left open
 *  - we do not allow extra tokens after macro directives
 *  - macro must be declared before use
 *
 * returns true on success, false on error
 */
static bool macro_scan(FILE *in, FILE *out, Macro **head) {
  char line[MAX_LINE_LENGTH];
  char copy[MAX_LINE_LENGTH];        /* local copy for tokenization */
  char macro_name[MAX_LABEL_LENGTH]; /* current macro name when inside */
  char *body = NULL;                 /* growing buffer for macro body */
  size_t body_len = 0;               /* used bytes in body */
  size_t body_cap = 0;               /* allocated bytes in body */

  int line_number = 0;  /* current input line number */
  int start_line = 0;   /* first line number of current macro */
  int inside_macro = 0; /* 0 = not inside, 1 = inside */

  /* line temporaries (per line) */
  char *token = NULL;
  size_t newline_position = 0;

  /* read lines one by one */
  while (fgets(line, (int)sizeof(line), in)) {
    line_number++;

    /* replace newlines with \0 so comparisons are simpler */
    newline_position = strcspn(line, "\n");
    line[newline_position] = '\0';

    /* prepare a copy for strtok (strtok modifies buffer) */
    strcpy(copy, line);

    /* first token on the line (space or tab separated) */
    token = strtok(copy, " \t");
    if (!token) {
      /* we don't really need it because we trim, but this is here for safetey,
       * if we have a blank line - we skip */
      continue;
    }

    /* we're inside a macro, so we collect its body */
    if (inside_macro) {
      /* end directive closes the macro */
      if (strcmp(token, MACRO_END_DIRECTIVE) == 0) {

        /* ensure there is no extra text after the end directive */
        if (has_extra_after_macro(line)) {
          fprintf(stderr, "(ERROR) [preprocessor] extra text after macro name "
                          "definition\n");
          free(body);
          return false;
        }

        /* finalize and store the macro (alloc checks inside) */
        if (!end_macro_definition(macro_name, &body, &body_len, &body_cap,
                                  start_line, head)) {
          /* end_macro_definition already printed a message if needed */
          return false;
        }

        inside_macro = 0;
        continue;
      }

      /* 'normal' body line - append to growing buffer with a newline */
      if (!append_body_line(&body, &body_len, &body_cap, line)) {
        fprintf(
            stderr,
            "(ERROR) [preprocessor] reallocating memory for macro failed\n");
        return false;
      }

      continue;
    } /* end if inside_macro */

    /* if we're outside a macro
     * maybe we're starting one with 'mcro X', so we check */
    if (strcmp(token, MACRO_START_DIRECTIVE) == 0) {
      if (!begin_macro_definition(line, line_number, head, macro_name,
                                  &start_line)) {
        /* begin_macro_definition prints the specific error */
        return false;
      }

      inside_macro = 1;
      continue;
    }

    /* if we're outside a macro, and we "end" it with 'mcroend' */
    if (strcmp(token, MACRO_END_DIRECTIVE) == 0) {
      fprintf(stderr,
              "(ERROR) [preprocessor] - macro without name definition\n");
      return false;
    }

    /* macro OR a normal line */
    /* we pass the first_token (i.e. the name of the macro) to the function */
    if (!expand_macro_or_emit_line(line, token, out, head, line_number)) {
      /*  expand_macro_or_emit_line prints the specific error */
      return false;
    }
  }

  /* reached eof. if a macro is still open, it is an error */
  if (inside_macro) {
    fprintf(stderr, "(ERROR) [preprocessor] macro without name definition\n");
    free(body);
    return false;
  }

  return true;
}
