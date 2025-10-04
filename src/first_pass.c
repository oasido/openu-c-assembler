#include "first_pass.h"
#include "assembler.h"
#include "data_image.h"
#include "helpers.h"
#include "instruction_image.h"
#include "instruction_utils.h"
#include "symbol_table.h"
#include "types.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *sanitize_line(char *line);
static bool check_label_legality(char *name, int line_number);
static char *consume_label_prefix(char *line, int line_number, char *label_out,
                                  bool *has_label);
static int handle_data_directive(char *operands, int *dc, char *label,
                                 int line_number, int *error_count);
static int handle_string_directive(char *operands, int *dc, char *label,
                                   int line_number, int *error_count);
static int handle_mat_directive(char *operands, int *dc, char *label,
                                int line_number, int *error_count);
static int handle_extern_directive(char *operands, Symbol **sym_table,
                                   char *label, int line_number,
                                   int *error_count);
static int handle_entry_directive(char *operands, char *label, int line_number,
                                  int *error_count);

/* major steps */
static int process_directive(char *directive, char *operands, char *label,
                             Symbol **sym_table, int *dc, int line_number,
                             int *error_count);
static int process_instruction(char *line, bool has_label, char *label_name,
                               Symbol **sym_table, int *IC, int line_num,
                               int *err_count);
static void relocate_data_symbols(Symbol *sym_table, int icf);

/* ======================================================================= */
/* ===================== public api -- see header ======================== */
/* ======================================================================= */

int first_pass(FILE *input_file, Symbol **sym_table, int *icf, int *dcf) {
  char raw_line[MAX_LINE_LENGTH];
  int line_number = 0;
  int ic = IC_INIT_VALUE;
  int dc = DC_INIT_VALUE;
  int error_count = 0;

  /* symbol table starts empty (sym_table points to NULL) */

  /* read each line from the .am file */
  while (fgets(raw_line, MAX_LINE_LENGTH, input_file)) {
    char *line = sanitize_line(raw_line); /* also replace newline with null */
    char *directive;
    char *operands;
    char label[MAX_LABEL_LENGTH];
    bool has_label = false;
    line_number++;

    /* skip empty lines */
    if (*line == '\0')
      continue;

    /* consume_label_prefix writes to label_out if a label was detected */
    directive = consume_label_prefix(line, line_number, label, &has_label);

    /* split directive&operands */
    directive = strtok(directive, " \t");
    operands = strtok(NULL, ""); /* everything after it */

    if (!directive) { /* invalid */
      error_count++;

      /* keep going so we can report more errors later */
      continue;
    }

    /* handle directives (.data/.string/.extern/.entry/.mat) */
    if (process_directive(directive, operands, has_label ? label : NULL,
                          sym_table, &dc, line_number, &error_count)) {
      continue;
    }

    /* otherwise it's an instruction line */
    {
      char inst_line[MAX_LINE_LENGTH];

      /* build the line */
      if (operands)
        sprintf(inst_line, "%s %s", directive, operands);
      else
        sprintf(inst_line, "%s", directive);

      /* process it */
      process_instruction(inst_line, has_label, has_label ? label : NULL,
                          sym_table, &ic, line_number, &error_count);
    }
  }

  /* set final instruction and data counters */
  *icf = ic;
  *dcf = dc;

  /* update data symbol addresses for code section placement,
   * data symbols start at icf address (AFTER all code), so we add icf offset */
  relocate_data_symbols(*sym_table, *icf);

  return error_count ? 1 : 0;
}

/* ======================================================================= */
/* ========================== static helpers ============================== */
/* ======================================================================= */

/* process_directive -- handles data directives (.data, .string, .mat) during
 * first pass
 *
 * returns 0 on success, 1 on error */
static int process_directive(char *directive, char *operands, char *label,
                             Symbol **sym_table, int *dc, int line_number,
                             int *error_count) {
  /* handle .data, .string, .mat directives and record label in symbol table
   */
  if (strcmp(directive, ".data") == 0 || strcmp(directive, ".string") == 0 ||
      strcmp(directive, ".mat") == 0) {

    /* add label */
    if (label) {
      if (!add_symbol(sym_table, label, *dc, SYMBOL_DATA))
        (*error_count)++;
    }

    /* if no operands after directives */
    if (!operands) {
      fprintf(stderr, "(ERROR) [first_pass] missing operand(s) in line %d\n",
              line_number);
      (*error_count)++;
      return 1;
    }
  }

  check_trailing_comma(operands, line_number, error_count);

  if (strcmp(directive, ".data") == 0)
    return handle_data_directive(operands, dc, label, line_number, error_count);

  if (strcmp(directive, ".string") == 0)
    return handle_string_directive(operands, dc, label, line_number,
                                   error_count);

  if (strcmp(directive, ".mat") == 0)
    return handle_mat_directive(operands, dc, label, line_number, error_count);

  if (strcmp(directive, ".extern") == 0) {
    return handle_extern_directive(operands, sym_table, label, line_number,
                                   error_count);
  }

  if (strcmp(directive, ".entry") == 0) {
    return handle_entry_directive(operands, label, line_number, error_count);
  }

  return 0;
}

/* process_instruction -- parse one instruction line, validate it, calc
 * length, and emit the opcode/operands words
 *
 * we can encode parts nOT depending on label values. words that need symbol
 * addresses get placeholders (and then we fix later)
 *
 * - first word:
 *   [9..6]=opcode, [5..4]=src mode, [3..2]=dst mode, [1..0]=A/R/E
 */
static int process_instruction(char *line, bool has_label, char *label_name,
                               Symbol **sym_table, int *IC, int line_num,
                               int *err_count) {
  char *opcode_str = NULL;
  char *operands_str = NULL;
  char *src = NULL, *dst = NULL;
  int opcode;
  int src_mode = -1, dst_mode = -1;
  int start_ic = *IC;
  int L = 0;
  const InstructionInfo *info;

  /* split line into opcode and the rest (operands text) */
  if (!parse_opcode_and_operands(line, &opcode_str, &operands_str)) {
    fprintf(stderr, "(ERROR) [first_pass] missing opcode at line %d\n",
            line_num);
    (*err_count)++;
    return 1;
  }

  /* map opcode, get its "data" (allowed modes, operand expectation) */
  opcode = opcode_from_string(opcode_str);
  if (opcode < 0) {
    fprintf(stderr, "(ERROR) [first_pass] unknown opcode '%s' at line %d\n",
            opcode_str ? opcode_str : "", line_num);
    (*err_count)++;
    return 1;
  }
  info = get_instruction_info(opcode);

  /* split src,dst (<= one comma& trim both sides) */
  if (!parse_two_operands(operands_str, &src, &dst, line_num, err_count)) {
    /* too many operands (error reported in parse_two_operands) */
    return 1;
  }

  /* check expected vs actual operands count */
  if (!check_operand_count(info, src, dst, line_num, err_count)) {
    /* error reported in check_operand_count */
    return 1;
  }

  /* compute addressing modes and check legality vs opcode */
  if (!validate_operand_modes(info, src, dst, &src_mode, &dst_mode, line_num,
                              err_count)) {
    /* error reported in validate_operand_modes */
    return 1;
  }

  /* define label (if present) */
  if (has_label) {
    if (!add_symbol(sym_table, label_name, start_ic, SYMBOL_CODE)) {
      (*err_count)++;
      /* we keep going on fail */
    }
  }

  /* compute total words (L) this instruction will take */
  L = compute_instruction_length(src_mode, dst_mode, src, dst);

  /* record the command for pass-2 / listing */
  if (!record_command(start_ic, L, opcode, src, dst,
                      has_label ? label_name : NULL)) {

    fprintf(stderr,
            "(ERROR) [first_pass] internal: record_command failed at line %d\n",
            line_num);
    (*err_count)++;
    /* continue */
  }

  /* EMITTING */
  /* emit the first word (opcode + modes)
   * TODO: A/R/E left 0 for now */
  if (!emit_first_word(opcode, src_mode, dst_mode, IC)) {
    fprintf(
        stderr,
        "(ERROR) [first_pass] internal: emit_first_word failed at line %d\n",
        line_num);
    (*err_count)++;
    /* continue; try to emit operands anyway for consistent IC advance */
  }

  /* emit operands; on numeric issues it reports but we do not hard-fail */
  if (!emit_operands(src, src_mode, dst, dst_mode, IC, line_num, err_count)) {
    /* keep original: errors counted, but function returns 0 */
  }

  return 0;
}

/* relocate_data_symbols -- add icf to addresses of all data symbols
 *
 * becuase data segment starts *after* the last code word
 */
static void relocate_data_symbols(Symbol *sym_table, int icf) {
  Symbol *s;

  for (s = sym_table; s; s = s->next) {
    if (s->type == SYMBOL_DATA) {
      s->address += icf;
    }
  }
}

/* ======================================================================= */

/* sanitize_line -- trim the trailing newline, strip comments, and clean up
 * whitespace. this uses cleanup_line() to remove ';' comments and normalize
 * spaces within the line
 */
static char *sanitize_line(char *line) {
  line[strcspn(line, "\n")] = '\0';
  cleanup_line(line);
  return line;
}

/* check_label_legality -- checks isalpha for first char, isalnum for rest,
 * and ensures that the label does not use saved keywords (such as 'mov')
 */
static bool check_label_legality(char *name, int line_number) {
  char *ptr = name;

  /* label must start with a letter */
  /* we cast to unsigned char to ensure we get a positive value */
  if (!isalpha((unsigned char)ptr[0])) {
    fprintf(stderr,
            "(ERROR) [first_pass] label '%s' at line %d must start with a "
            "letter\n",
            name, line_number);
    return false;
  }

  /* remaining chars must also be alphanumeric */
  ptr++;

  while (*ptr) {
    if (!isalnum((unsigned char)*ptr)) {
      fprintf(stderr,
              "(ERROR) [first_pass] label '%s' at line %d contains an invalid "
              "character\n",
              name, line_number);
      return false;
    }
    ptr++;
  }

  return is_illegal_name(name) ? false : true;
}

/* consume_label_prefix -- checks for labels, if found it checks whether
 * they're named correctly (alpha first char, then the rest alphanumeric). it
 * also checks if the label uses a saved program keyword (such as 'mov')
 */
static char *consume_label_prefix(char *line, int line_number, char *label_out,
                                  bool *has_label) {
  char *colon;

  if (*has_label == true) {
    *has_label = false;
  }

  colon = strchr(line, ':');
  if (!colon) {
    return line; /* no label */
  }

  /* temporarily terminate label */
  *colon = '\0';

  /* check label length before any buffer operations */
  if (strlen(line) >= MAX_LABEL_LENGTH) {
    fprintf(stderr,
            "(ERROR) [first_pass] label '%s' at line %d exceeds maximum length "
            "of %d characters\n",
            line, line_number, MAX_LABEL_LENGTH - 1);

    /* restore the colon and skip to content after colon */
    *colon = ':';
    return colon + 1;
  }

  if (!check_label_legality(line, line_number)) {
    fprintf(stderr, "(ERROR) [first_pass] illegal label found: '%s'\n", line);
    /* restore the colon and skip to content after colon */
    *colon = ':';
    return colon + 1;
  }

  /* safe copy since we've verified the length */
  strcpy(label_out, line);
  if (has_label)
    *has_label = true;

  /* return pointer just after the colon */
  return colon + 1;
}

/* handle_data_directive -- parse .data operands and store them in the
 * directive list.
 */
static int handle_data_directive(char *operands, int *dc, char *label,
                                 int line_number, int *error_count) {
  char *delim = "\t ,";
  char *copy = safe_strdup(operands);
  char *tok;
  int count = 0;
  DirectiveFields *df = NULL;
  int idx = 0;

  /* make a working copy because strtok() modifies the buffer */
  if (!copy) {
    fprintf(stderr,
            "(ERROR) [first_pass] memory allocation failed at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  /* validate each number and count how many values there are */
  for (tok = strtok(copy, delim); tok; tok = strtok(NULL, delim)) {
    if (!is_valid_data_num(tok)) {
      fprintf(stderr,
              "(ERROR) [first_pass] invalid number in .data at line %d near "
              "'%s'\n",
              line_number, tok);
      (*error_count)++;
    }
    count++;
  }

  free(copy);

  /* .data must have at least one value */
  if (count <= 0) {
    fprintf(
        stderr,
        "(ERROR) [first_pass] .data requires at least one value at line %d\n",
        line_number);
    (*error_count)++;
    return 1;
  }

  df = new_directive(count);

  if (!df) {
    fprintf(stderr,
            "(ERROR) [first_pass] memory allocation failed at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  df->label = label ? safe_strdup(label) : NULL;
  df->arg_label = NULL;
  df->is_extern = false;
  df->is_entry = false;
  df->data_address = *dc;

  for (tok = strtok(operands, delim); tok; tok = strtok(NULL, delim)) {
    short val = (short)atoi(tok);

    if (!is_num_within_range(val)) {
      fprintf(stderr,
              "(ERROR) [first_pass] number out of range in .data at line %d "
              "near '%s'\n",
              line_number, tok);
      (*error_count)++;
    }
    df->data[idx++] = val;
    (*dc)++;
  }

  append_directive(df);

  return 1;
}

/* handle_string_directive -- parse .string operands, allocate and populate a
 * DirectiveFields entry and append to list
 */
static int handle_string_directive(char *operands, int *dc, char *label,
                                   int line_number, int *error_count) {
  char *p = trim_left(operands);
  char *start = strchr(p, '"');
  char *end = NULL;
  DirectiveFields *df = NULL;
  int len = 0;
  int idx;

  if (!start) {
    fprintf(stderr,
            "(ERROR) [first_pass] .string missing opening quote at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  start++;

  end = strchr(start, '"');
  if (!end) {
    fprintf(stderr,
            "(ERROR) [first_pass] .string missing closing quote at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  /* ensure no extra text follows the closing quote */
  {
    char *trail = end + 1;

    while (*trail && isspace(*trail))
      trail++;

    if (*trail) {
      fprintf(
          stderr,
          "(ERROR) [first_pass] extra text after closing quote at line %d\n",
          line_number);
      (*error_count)++;
    }
  }

  len = (int)(end - start);
  df = new_directive(len + 1);

  if (!df) {
    fprintf(stderr,
            "(ERROR) [first_pass] memory allocation failed at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  df->label = label ? safe_strdup(label) : NULL;
  df->arg_label = NULL;
  df->is_extern = false;
  df->is_entry = false;
  df->data_address = *dc;

  /* populate data */
  for (idx = 0; idx < len; idx++) {
    df->data[idx] = (short)start[idx];
    (*dc)++;
  }
  df->data[len] = 0;

  (*dc)++;

  append_directive(df);

  return 1;
}

/* handle_mat_directive -- parse .mat operands, allocate and populate a
 * DirectiveFields entry and append to list
 */
static int handle_mat_directive(char *operands, int *dc, char *label,
                                int line_number, int *error_count) {
  int rows = 0;
  int cols = 0;
  int consumed_chars = 0;
  int maximum_cells = 0;
  char *copy = NULL;
  char *tok = NULL;
  int count = 0;
  DirectiveFields *df = NULL;
  int idx = 0;

  /* attempt to read rows & cols, %n will give us the pointer offset */
  if (sscanf(operands, " [%d] [%d] %n", &rows, &cols, &consumed_chars) != 2 ||
      rows <= 0 || cols <= 0) {
    fprintf(stderr,
            "(ERROR) [first_pass] .mat expects dimensions [r][c] at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  /* update the maximum cells */
  maximum_cells = rows * cols;

  /* [r][c] v1 */
  /* ------^   */
  operands += consumed_chars;

  /* make a copy because strtok() modifies the buffer */
  copy = safe_strdup(operands);
  if (!copy) {
    fprintf(stderr,
            "(ERROR) [first_pass] memory allocation failed at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  count = 0;

  for (tok = strtok(copy, "\t ,"); tok; tok = strtok(NULL, "\t ,")) {
    if (!is_valid_data_num(tok)) {
      fprintf(stderr,
              "(ERROR) [first_pass] invalid number in .mat at line %d near "
              "'%s'\n",
              line_number, tok);
      (*error_count)++;
    }
    count++;
  }

  free(copy);

  /* check the number of values passed */
  if (maximum_cells == 0 && count > 0) {
    fprintf(stderr,
            "(ERROR) [first_pass] .mat has zero cells but %d values were given "
            "at line %d\n",
            count, line_number);
    (*error_count)++;
  } else if (count > maximum_cells) {
    fprintf(stderr,
            "(ERROR) [first_pass] .mat has %d values but maximum is %d at line "
            "%d\n",
            count, maximum_cells, line_number);
    (*error_count)++;
  }

  /* create a new directive */
  df = new_directive(maximum_cells);
  if (!df) {
    fprintf(stderr,
            "(ERROR) [first_pass] memory allocation failed at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  /* populate the directive */
  df->label = label ? safe_strdup(label) : NULL;
  df->arg_label = NULL;
  df->is_extern = false;
  df->is_entry = false;
  df->data_address = *dc;
  idx = 0;

  /* fill matrix data */
  for (tok = strtok(operands, "\t ,"); tok; tok = strtok(NULL, "\t ,")) {
    short val = (short)atoi(tok);

    /* check if val is within range */
    if (!is_num_within_range(val)) {
      fprintf(stderr,
              "(ERROR) [first_pass] number out of range in .mat at line %d "
              "near '%s'\n",
              line_number, tok);
      (*error_count)++;
    }
    df->data[idx++] = val;
    (*dc)++;
  }

  append_directive(df);

  return 1;
}

/* handle_extern_directive -- parse .extern operands, allocate and populate a
 * DirectiveFields entry and append to list
 */
static int handle_extern_directive(char *operands, Symbol **sym_table,
                                   char *label, int line_number,
                                   int *error_count) {
  char *name = safe_strdup(operands);
  DirectiveFields *df = NULL;

  if (label) {
    fprintf(stderr,
            "(WARNING) [first_pass] label before .extern is ignored at "
            "line %d\n",
            line_number);
  }

  if (!name) {
    fprintf(stderr,
            "(ERROR) [first_pass] .extern requires a symbol name at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  if (!add_symbol(sym_table, name, 0, SYMBOL_EXTERNAL))
    (*error_count)++;

  df = new_directive(0);
  if (!df) {
    fprintf(stderr,
            "(ERROR) [first_pass] memory allocation failed at line %d\n",
            line_number);
    (*error_count)++;
    return 1;
  }

  df->arg_label = name;
  df->is_extern = true;
  df->is_entry = false;
  df->label = NULL;
  df->data_address = -1;
  append_directive(df);

  return 1;
}

/* handle_entry_directive -- parse .entry operands */
static int handle_entry_directive(char *operands, char *label, int line_number,
                                  int *error_count) {
  char *name = safe_strdup(operands);
  DirectiveFields *df = NULL;

  if (label) {
    fprintf(stderr,
            "(WARNING) [first_pass] label before .entry is ignored at "
            "line %d\n",
            line_number);
  }

  if (!name) {
    fprintf(stderr,
            "(ERROR) [first_pass] .entry requires a symbol name at line %d\n",
            line_number);
    (*error_count)++;
    free(name);
    return 1;
  }

  df = new_directive(0);
  if (!df) {
    fprintf(stderr,
            "(ERROR) [first_pass] memory allocation failed at line %d\n",
            line_number);
    free(name);
    (*error_count)++;
    return 1;
  }

  df->arg_label = name;
  df->is_extern = false;
  df->is_entry = true;
  df->label = NULL;
  df->data_address = -1;
  append_directive(df);
  return 1;
}
