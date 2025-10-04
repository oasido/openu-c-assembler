#include "second_pass.h"
#include "assembler.h"
#include "data_image.h"
#include "first_pass.h"
#include "helpers.h"
#include "instruction_image.h"
#include "instruction_utils.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* second_pass.c -- implementation of the assembler second pass */

static void mark_word_absolute(int idx);
static void write_external_reference(FILE **ext_fp, const char *base_filename,
                                     const char *sym_name, int idx);
static void encode_symbol_word(int idx, Symbol *sym, FILE **ext_fp,
                               const char *base_filename);
static Symbol *find_matrix_symbol(Symbol *symtab, const char *operand,
                                  int *error_count);
static void resolve_direct_operand(const char *label, int idx, Symbol *symtab,
                                   FILE **ext_fp, const char *base_filename,
                                   int *error_count);
static void resolve_matrix_operand(const char *operand, int *idx,
                                   Symbol *symtab, FILE **ext_fp,
                                   const char *base_filename, int *error_count);
static void resolve_operand(const char *operand, int mode, int *idx,
                            Symbol *symtab, FILE **ext_fp,
                            const char *base_filename, int *error_count);

/* major steps */
static void resolve_symbols(Symbol *symtab, CommandFields *commands[],
                            int command_count, FILE **ext_fp,
                            const char *base_filename, int *error_count);
static void write_object(FILE *ob_fp, int icf, DirectiveFields *directives[],
                         int directive_count);
static void write_entries(const char *base_filename, Symbol *symtab,
                          DirectiveFields *directives[], int directive_count,
                          int *error_count);

/* ======================================================================= */
/* ===================== public api -- see header ======================== */
/* ======================================================================= */

int second_pass(Symbol *symtab, int icf, const char *base_filename) {
  FILE *ob_fp = NULL;
  FILE *ext_fp = NULL;
  int error_count = 0;

  /* we open .ob first
   * .ext/.ent files are opened later if when needed */
  ob_fp = open_file_with_ext(base_filename, ".ob", "w");
  if (!ob_fp) {
    fprintf(stderr, "(ERROR) [second_pass] failed to create object file\n");
    return -1;
  }

  /* resolve symbolic operands into the image */
  resolve_symbols(symtab, command_list, command_count, &ext_fp, base_filename,
                  &error_count);

  /* write object file (code then data) */
  write_object(ob_fp, icf, directive_list, directive_count);
  fclose(ob_fp);

  /* write entries file if any entries exist */
  write_entries(base_filename, symtab, directive_list, directive_count,
                &error_count);

  /* close .ext if it was opened during resolve */
  if (ext_fp) {
    fclose(ext_fp);
  }

  return error_count;
}

/* ======================================================================= */
/* ========================== static helpers ============================== */
/* ======================================================================= */

/* resolve_symbols -- fixes symbol references in operands during second pass
 *
 * looks up symbols for direct and matrix addressing modes, writes external
 * references to .ext file, updates instruction image with resolved addresses */
static void resolve_symbols(Symbol *symtab, CommandFields *commands[],
                            int command_count, FILE **ext_fp,
                            const char *base_filename, int *error_count) {
  int i;

  /* iterate each command emitted by first pass */
  for (i = 0; i < command_count; i++) {
    CommandFields *cmd;
    char *src;
    char *dst;
    int src_mode;
    int dst_mode;
    int idx;

    cmd = commands[i];
    if (!cmd) {
      /* skip null slots */
      continue;
    }

    /* read operands and their addressing modes */
    src = cmd->src;
    dst = cmd->dst;

    src_mode = src ? addr_mode(src) : -1;
    dst_mode = dst ? addr_mode(dst) : -1;

    /* extra words start right after the opcode word */
    idx = cmd->cmd_address - IC_INIT_VALUE + 1;

    /* if both operands are registers we have one packed absolute word holding
     * both */
    if (src && dst && src_mode == ADDR_MODE_REGISTER &&
        dst_mode == ADDR_MODE_REGISTER) {
      mark_word_absolute(idx);
      idx++; /* advance */
    } else {
      /* resolve src if present (may advance by 1 or 2 for matrix) */
      if (src) {
        resolve_operand(src, src_mode, &idx, symtab, ext_fp, base_filename,
                        error_count);
      }

      /* resolve dst if present */
      if (dst) {
        resolve_operand(dst, dst_mode, &idx, symtab, ext_fp, base_filename,
                        error_count);
      }
    }
  }
}

/* write_object (file) -- dump code image FIRST, then data image to .ob */
static void write_object(FILE *ob_fp, int icf, DirectiveFields *directives[],
                         int directive_count) {
  int i;

  /* code segment: print each code word in base-4 letter format */
  for (i = 0; i < (icf - IC_INIT_VALUE); i++) {
    int addr = IC_INIT_VALUE + i;
    char *addr_letters = decimal_to_base4_letters(addr);
    char *code_letters = decimal_to_base4_letters(instruction_image[i]);

    /* write line in base-4 letter format */
    if (addr_letters && code_letters) {
      fprintf(ob_fp, "%s %s\n", addr_letters, code_letters);
    } else {
      /* fallback on allocation failure */
      fprintf(ob_fp, "aaaaa aaaaa\n");
    }

    /* cleanup memory */
    if (addr_letters)
      free(addr_letters);
    if (code_letters)
      free(code_letters);
  }

  /* data segment - follows code starting at icf */
  for (i = 0; i < directive_count; i++) {
    DirectiveFields *df = directives[i];
    int j;

    if (!df || df->data_length <= 0) {
      /* nothing to print here */
      continue;
    }

    /* each data value occupies one word */
    for (j = 0; j < df->data_length; j++) {
      int addr = icf + df->data_address + j;
      char *addr_letters = decimal_to_base4_letters(addr);
      char *data_letters = decimal_to_base4_letters(df->data[j]);

      /* write line in base-4 letter format */
      if (addr_letters && data_letters) {
        fprintf(ob_fp, "%s %s\n", addr_letters, data_letters);
      } else {
        /* fallback on allocation failure */
        fprintf(ob_fp, "aaaaa aaaaa\n");
      }

      /* cleanup memory */
      if (addr_letters)
        free(addr_letters);
      if (data_letters)
        free(data_letters);
    }
  }
}

/* write_entries -- generate .ent with <label> <address> lines */
static void write_entries(const char *base_filename, Symbol *symtab,
                          DirectiveFields *directives[], int directive_count,
                          int *error_count) {
  FILE *ent_fp = NULL;
  int idx;

  /* scan directives for entry declarations (not extern) */
  for (idx = 0; idx < directive_count; idx++) {
    DirectiveFields *df = directives[idx];
    Symbol *sym;
    char *addr_letters;

    if (!df || df->is_extern || !df->arg_label) {
      continue;
    }

    /* resolve the entry label in the symbol table */
    sym = find_symbol(symtab, df->arg_label);
    if (!sym) {
      fprintf(stderr, "(ERROR) [second_pass] entry symbol '%s' not found\n",
              df->arg_label);
      (*error_count)++;
      continue;
    }

    /* open .ent on first need */
    if (!ent_fp) {
      ent_fp = open_file_with_ext(base_filename, ".ent", "w");
      if (!ent_fp) {
        fprintf(stderr, "(ERROR) [second_pass] failed to open .ent file\n");
        (*error_count)++;
        /* continue to report other entries errors */
        continue;
      }
    }

    /* write one row - name and final address in base4 letters */
    addr_letters = decimal_to_base4_letters(sym->address);
    if (addr_letters) {
      fprintf(ent_fp, "%s %s\n", sym->name, addr_letters);

      /* cleanup memory */
      free(addr_letters);
    } else {
      /* fallback on allocation failure */
      fprintf(ent_fp, "%s aaaaa\n", sym->name);
    }
  }

  if (ent_fp) {
    fclose(ent_fp);
  }
}

/* ======================================================================= */

/* mark_word_absolute -- set A/R/E bits to 00 at index in code image */
static void mark_word_absolute(int idx) {
  int word;

  /* read current word */
  word = instruction_image[idx];

  /* clear ARE bits using mask, 00 = ABSOLUTE */
  word = word & (~ARE_MASK);

  /* write back */
  instruction_image[idx] = word;
}

/* write_external_reference -- append "<symbol> <address>" to .ext
 */
static void write_external_reference(FILE **ext_fp, const char *base_filename,
                                     const char *sym_name, int idx) {
  /* open .ext on first use, if we have the file pointer we skip */
  if (!*ext_fp) {
    *ext_fp = open_file_with_ext(base_filename, ".ext", "w");
  }

  /* if file is open, insert this */
  if (*ext_fp) {
    char *addr_letters = decimal_to_base4_letters(IC_INIT_VALUE + idx);
    if (addr_letters) {
      fprintf(*ext_fp, "%s %s\n", sym_name, addr_letters);

      /* cleanup memory */
      free(addr_letters);
    } else {
      /* fallback on allocation failure */
      fprintf(*ext_fp, "%s aaaaa\n", sym_name);
    }
  }
}

/* encode_symbol_word -- fill extra word for DIRECT/MATRIX based on symbol attrs
 */
static void encode_symbol_word(int idx, Symbol *sym, FILE **ext_fp,
                               const char *base_filename) {

  if (sym->type == SYMBOL_EXTERNAL) {
    /* external symbol processing */
    /* payload is unknown here, so 0. ARE bits are 01 (external) */
    instruction_image[idx] = ARE_EXTERNAL;

    /* write to .ext */
    write_external_reference(ext_fp, base_filename, sym->name, idx);
  } else {
    /* this is relocatable - address payload with ARE=10 */
    /* symbol is defined within current source file */
    /* mask to valid word size */
    int addr = sym->address & WORD_MASK;

    /* pack address into bits 2-9 and set A/R/E to 10 (relocatable) */
    instruction_image[idx] = (addr << ADDRESS_PAYLOAD_SHIFT) | ARE_RELOCATABLE;
  }
}

/* find_matrix_symbol -- pull LABEL from "LABEL[rX][rY]" and lookup */
static Symbol *find_matrix_symbol(Symbol *symtab, const char *operand,
                                  int *error_count) {
  const char *bracket;
  size_t len;
  char label_buf[MAX_LABEL_LENGTH + 1];
  Symbol *sym = NULL;

  /* find the first '[' which ends the label part */
  bracket = strchr(operand, '[');
  if (!bracket || bracket == operand) {
    fprintf(stderr, "(ERROR) [second_pass] invalid matrix operand '%s'\n",
            operand);
    (*error_count)++;
    return NULL;
  }

  /* copy the label portion */
  len = (size_t)(bracket - operand);
  if (len > MAX_LABEL_LENGTH) {
    len = MAX_LABEL_LENGTH;
  }
  strncpy(label_buf, operand, len);
  label_buf[len] = '\0';
  trim(label_buf);

  /* lookup in symbol table */
  sym = find_symbol(symtab, label_buf);
  if (!sym) {
    fprintf(stderr, "(ERROR) [second_pass] undefined symbol '%s'\n", label_buf);
    (*error_count)++;
  }
  return sym;
}

/* resolve_direct_operand -- encode one DIRECT label reference at idx */
static void resolve_direct_operand(const char *label, int idx, Symbol *symtab,
                                   FILE **ext_fp, const char *base_filename,
                                   int *error_count) {
  Symbol *sym;

  /* resolve label */
  sym = find_symbol(symtab, (char *)label);
  if (!sym) {
    fprintf(stderr, "(ERROR) [second_pass] undefined symbol '%s'\n", label);
    (*error_count)++;
    /* leave a zero as a safe default, even if invalid.. */
    instruction_image[idx] = 0;
    return;
  }

  /* write R (10)/E (01) encoded word */
  encode_symbol_word(idx, sym, ext_fp, base_filename);
}

/* resolve_matrix_operand -- two words: base label (R/E) then absolute indices
 */
static void resolve_matrix_operand(const char *operand, int *idx,
                                   Symbol *symtab, FILE **ext_fp,
                                   const char *base_filename,
                                   int *error_count) {
  Symbol *sym;

  /* first extra word is base address for the matrix */
  sym = find_matrix_symbol(symtab, operand, error_count);
  if (!sym) {
    instruction_image[*idx] = 0;
  } else {
    encode_symbol_word(*idx, sym, ext_fp, base_filename);
  }
  (*idx)++;

  /* second extra word has only register indices -> absolute */
  mark_word_absolute(*idx);
  (*idx)++;
}

/* resolve_operand -- handle by addressing mode, advance idx accordingly */
static void resolve_operand(const char *operand, int mode, int *idx,
                            Symbol *symtab, FILE **ext_fp,
                            const char *base_filename, int *error_count) {
  switch (mode) {
  case ADDR_MODE_REGISTER:
  case ADDR_MODE_IMMEDIATE:
    /* immediate (00)/register (11) - no symbol to resolve
     * we mark this extra word absolute (ARE=00) and move to the next slot */
    mark_word_absolute(*idx);
    (*idx)++;
    break;

  case ADDR_MODE_DIRECT:
    /* direct (01) - needs symbol resolution */
    resolve_direct_operand(operand, *idx, symtab, ext_fp, base_filename,
                           error_count);
    /* also writes either relocatable (ARE=10) or external (ARE=01) */
    (*idx)++;
    break;

  case ADDR_MODE_MATRIX:
    /* matrix - two words total
     * first word is the base label (like direct), second is indices (absolute)
     */
    resolve_matrix_operand(operand, idx, symtab, ext_fp, base_filename,
                           error_count);
    /* handle both writes and increases idx by 2 */
    break;

  default:
    /* unknown
     * to not get stuck we increment idx
     * should we add error? */
    (*idx)++;
    break;
  }
}
