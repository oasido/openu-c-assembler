#include "instruction_image.h"
#include "assembler.h"
#include "data_image.h"
#include "first_pass.h"
#include "helpers.h"
#include "instruction_utils.h"
#include <stdio.h>
#include <stdlib.h>

/* instruction_image -- keeps the code image (10 bit words) and a simple
 * list of parsed commands */

static int encode_immediate8(int val);
static int encode_reg_src_word(int reg_code_val);
static int encode_reg_dst_word(int reg_code_val);
static int encode_regs_shared(int src_reg_code_val, int dst_reg_code_val);
static int encode_matrix_indices(int row_reg_code_val, int col_reg_code_val);

/* global instruction/code image (10-bit words) */
int instruction_image[MAX_WORDS_MEMORY];

/* registry of parsed commands to help later stages (second pass .. ) */
CommandFields *command_list[MAX_WORDS_MEMORY];

/* how many commands we have stored so far */
int command_count = 0;

/* ======================================================================= */
/* ===================== public api -- see header ======================== */
/* ======================================================================= */

void append_command(CommandFields *cf) {
  /* check for overflow before adding to list */
  /* this check is not accurate, that's why we have a similar check in
   * assembler.c */
  if (command_count + directive_count >= MAX_WORDS_MEMORY) {
    fprintf(
        stderr,
        "(ERROR) [instruction_image] command list overflow, dropping entry\n");
    free_command(cf);
    return;
  }

  /* insert command into the global registry & advance count by 1 after the
   * insertion */
  command_list[command_count++] = cf;
}

int emit_word(int value, int *IC) {
  /* convert IC to array index (IC starts at IC_INIT_VALUE=100, array at 0) */
  int idx = *IC - IC_INIT_VALUE;

  /* validate index bounds */
  if (idx < 0 || idx >= MAX_WORDS_MEMORY)
    return -1;

  /* mask to 10 bits and store in instruction image */
  instruction_image[idx] = value & WORD_MASK;

  /* advance instruction counter for next word */
  (*IC)++;

  return idx;
}

CommandFields *record_command(int start_ic, int length_words, int opcode,
                              const char *src, const char *dst,
                              const char *label_or_null) {
  /* allocate new command record */
  CommandFields *cf = new_command();
  if (!cf)
    return NULL;

  /* fill command data for second pass usage */
  cf->label = label_or_null ? safe_strdup(label_or_null) : NULL;
  cf->cmd_address = start_ic;
  cf->length = length_words;
  cf->opcode = opcode;
  cf->src = src ? safe_strdup(src) : NULL;
  cf->dst = dst ? safe_strdup(dst) : NULL;

  /* add to global command registry */
  append_command(cf);
  return cf;
}

int emit_first_word(int opcode, int src_mode, int dst_mode, int *IC) {
  int word;
  int opcode_part, src_mode_part, dst_mode_part;

  if (!IC)
    return 0;

  /* build the first word step by step - word layout:
   *  9 8 7 6 5 4 3 2 1 0
   *  [opcode][src][dst][A/R/E] */

  /* opcode goes in bits 9-6, so shift left by OPCODE_SHIFT (6) */
  opcode_part = (opcode & OPCODE_MASK) << OPCODE_SHIFT;

  /* ssrc_mode goes in bits 5-4, so shift left by SRC_MODE_SHIFT (4) */
  src_mode_part = (src_mode & ADDR_MODE_MASK) << SRC_MODE_SHIFT;

  /* dst_mode goes in bits 3-2, so shift left by DST_MODE_SHIFT (2) */
  dst_mode_part = (dst_mode & ADDR_MODE_MASK) << DST_MODE_SHIFT;

  /* combine all parts using bitwise OR */
  word = opcode_part | src_mode_part | dst_mode_part;
  /* note: A/R/E bits (1-0) remain 00 since we did not set them */

  /* emit the packed first word to instruction image */
  emit_word(word & WORD_MASK, IC);

  return 1;
}

bool emit_operands(const char *src, int src_mode, const char *dst, int dst_mode,
                   int *IC, int line_num, int *err_count) {
  int had_error = 0;

  if (src && dst && src_mode == ADDR_MODE_REGISTER &&
      dst_mode == ADDR_MODE_REGISTER) {
    /* if both operands are registers -> we use one shared extra word
       layout (two regs):
         bit:   9 8 7 6 5 4 3 2 1 0
                [  src reg  ][ dst ][A/R/E]
                    6..9       2..5   0..1
      also: we leave A/R/E/=00 here
    */
    int word = encode_regs_shared(reg_code(src), reg_code(dst));

    emit_word(word, IC);

    return true; /* done */
  }

  /* handle src if present */
  if (src) {
    if (src_mode == ADDR_MODE_REGISTER) {
      /* single-register source gets its own extra word
         layout (src register word):
           bit:   9 8 7 6 5 4 3 2 1 0
                  [  src reg  ][ 0 0 0 0 ][A/R/E]
                      6..9          2..5     0..1
         still, the A/R/E=00 bits are kept 0
      */
      int word = encode_reg_src_word(reg_code(src));
      emit_word(word, IC);
    } else if (src_mode == ADDR_MODE_IMMEDIATE) {
      /* immediate (#num) extra word encodes 1 word (8 data bits + A/R/E)
         layout (immediate word):
           bit:   9 8 7 6 5 4 3 2 1 0
                  [   imm[7:0]   ][A/R/E]
                       9..2          0..1
      */
      int val;

      /* validate immediate value syntax */
      if (!is_valid_data_num(src + 1)) {
        fprintf(stderr, "(ERROR) [first_pass] invalid immediate at line %d\n",
                line_num);
        if (err_count)
          (*err_count)++;
        had_error = 1;
      }

      val = atoi(src + 1);

      /* validate immediate value range (8-bit signed) */
      if (!validate_immediate_range(val, line_num, err_count)) {
        had_error = 1;
      }

      /* encode immediate with left shift by 2 to leave A/R/E=00 */
      emit_word(encode_immediate8(val), IC);
    } else if (src_mode == ADDR_MODE_DIRECT) {
      /* direct label: placeholder for now - (we emit 0)
       * resolved to address + proper A/R/E in the second pass
         layout (direct word):
          bit:   9 8 7 6 5 4 3 2 1 0
                  [ address/placeholder ][A/R/E]
                           9..2             0..1
      */
      emit_word(0, IC); /* placeholder .. */
    } else if (src_mode == ADDR_MODE_MATRIX) {
      /* matrix uses 2 extra words:
           word #1: base address of the matrix label (placeholder for now)
           word #2: packed (row reg, col reg) + A/R/E
         layout (matrix indices word):
           bit:   9 8 7 6 5 4 3 2 1 0
                  [ row reg ][ col ][A/R/E]
                     6..9      2..5   0..1
      */
      int row = 0, col = 0;

      /* first word - matrix base address (placeholder) */
      emit_word(0, IC);

      /* parse and validate matrix syntax: label[reg][reg] */
      if (!parse_matrix_regs(src, &row, &col)) {
        fprintf(stderr,
                "(ERROR) [first_pass] invalid matrix syntax at line %d\n",
                line_num);
        if (err_count)
          (*err_count)++;
        had_error = 1;
      }

      /* second word - "packed" register indices */
      emit_word(encode_matrix_indices(row, col), IC);
    }
  }

  /* handle dst if present */
  /* NOTE: similar logic to src */
  if (dst) {
    if (dst_mode == ADDR_MODE_REGISTER) {
      /* single-register destination gets its own extra word.
         layout (dst register word):
            bit:   9 8 7 6 5 4 3 2 1 0
                  [ 0 0 0 0 ][ dst ][A/R/E]
                               2..5   0..1
         still, the A/R/E=00 bits are kept 0
      */
      int word = encode_reg_dst_word(reg_code(dst));
      emit_word(word, IC);
    } else if (dst_mode == ADDR_MODE_IMMEDIATE) {
      /* identical to src immediate:
         layout:
           bit:   9 8 7 6 5 4 3 2 1 0
                  [   imm[7:0]   ][A/R/E]
      */
      int val;
      if (!is_valid_data_num(dst + 1)) {
        fprintf(stderr, "(ERROR) [first_pass] invalid immediate at line %d\n",
                line_num);
        if (err_count)
          (*err_count)++;
        had_error = 1;
      }
      val = atoi(dst + 1);

      /* validate immediate value range (8-bit signed) */
      if (!validate_immediate_range(val, line_num, err_count)) {
        had_error = 1;
      }

      emit_word(encode_immediate8(val), IC);
    } else if (dst_mode == ADDR_MODE_DIRECT) {
      /* direct label (destination): same placeholder idea as src direct */
      emit_word(0, IC); /* placeholder */
    } else if (dst_mode == ADDR_MODE_MATRIX) {
      /* two extra words, same packing as src matrix */
      int row = 0, col = 0;
      emit_word(0, IC); /* label placeholder */
      if (!parse_matrix_regs(dst, &row, &col)) {
        fprintf(stderr,
                "(ERROR) [first_pass] invalid matrix syntax at line %d\n",
                line_num);
        if (err_count)
          (*err_count)++;
        had_error = 1;
      }
      emit_word(encode_matrix_indices(row, col), IC);
    }
  }

  return had_error ? false : true;
}

CommandFields *new_command(void) {
  return safe_calloc(1, sizeof(CommandFields));
}

void free_command(CommandFields *cf) {
  if (!cf)
    return;

  /* free all owned string memory */
  free(cf->label);
  free(cf->src);
  free(cf->dst);
  free(cf);
}

void free_commands(void) {
  int idx;

  /* iterate through all stored commands and free each one */
  for (idx = 0; idx < command_count; ++idx) {
    free_command(command_list[idx]);
    command_list[idx] = NULL;
  }

  /* reset command_count to 0 */
  command_count = 0;
}

/* ======================================================================= */
/* ========================== static helpers ============================== */
/* ======================================================================= */

/* encode_immediate8 -- packs an 8-bit immediate into bits 2..9, A/R/E left 00
 */
static int encode_immediate8(int val) {
  /* mask with 8 bits (IMM_MASK) and shift left by IMM_DATA_SHIFT
   * to place immediate data in bits 2-9, leaving A/R/E bits (0-1) as 00
   *
   * returns the word with A/R/E=00
   */
  return ((val & IMM_MASK) << IMM_DATA_SHIFT);
}

/* encode_reg_src_word -- packs a single src register into bits 6..9
    returns encoded extra word with src reg; other fields zero, A/R/E=00
*/
static int encode_reg_src_word(int reg_code_val) {
  /* pack source register nibble into bits 6..9 using REG_SRC_SHIFT */
  return (reg_code_val & NIBBLE_MASK) << REG_SRC_SHIFT;
}

/* encode_reg_dst_word -- packs a single *destination* register into bits 2..5
    ret: encoded extra word with dst reg; other fields zero, A/R/E=00
*/
static int encode_reg_dst_word(int reg_code_val) {
  /* pack dest‑reg nibble into bits 2..5 */
  return (reg_code_val & NIBBLE_MASK) << REG_DST_SHIFT;
}

/* encode_regs_shared -- packs src+dst registers into one *shared* extra word
    layout (10 bits total):
      bit:   9 8 7 6 5 4 3 2 1 0
             [  src reg  ][ dst ][A/R/E]
                 6..9       2..5   0..1
    ret: encoded shared word; A/R/E left 00
*/
static int encode_regs_shared(int src_reg_code_val, int dst_reg_code_val) {
  /* src in bits 6..9, dst in bits 2..5 → leaves A/R/E=00 */
  return ((src_reg_code_val & NIBBLE_MASK) << REG_SRC_SHIFT) |
         ((dst_reg_code_val & NIBBLE_MASK) << REG_DST_SHIFT);
}

/* encode_matrix_indices -- packs matrix row/col regs into bits (row 6..9,
   col 2..5) ret: encoded indices word; A/R/E left 00 note: indices/register ids
   are 0-based; only low 4 bits are used
*/
static int encode_matrix_indices(int row_reg_code_val, int col_reg_code_val) {
  /* row register in bits 6..9 using REG_SRC_SHIFT (6)
   * col register in bits 2..5 using REG_DST_SHIFT (2)
   * this leaves A/R/E bits (0-1) as 00 */
  return ((row_reg_code_val & NIBBLE_MASK) << REG_SRC_SHIFT) |
         ((col_reg_code_val & NIBBLE_MASK) << REG_DST_SHIFT);
}
