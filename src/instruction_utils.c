#include "instruction_utils.h"
#include "helpers.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* instruction_utils -- helpers for parsing and validating instructions */

/* instruction table mapping opcodes to their properties */
const InstructionInfo instruction_info_table[] = {
    {0, "mov",
     ADDR_MASK_IMMEDIATE | ADDR_MASK_DIRECT | ADDR_MASK_MATRIX |
         ADDR_MASK_REGISTER,
     ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},

    {1, "cmp",
     ADDR_MASK_IMMEDIATE | ADDR_MASK_DIRECT | ADDR_MASK_MATRIX |
         ADDR_MASK_REGISTER,
     ADDR_MASK_IMMEDIATE | ADDR_MASK_DIRECT | ADDR_MASK_MATRIX |
         ADDR_MASK_REGISTER},

    {2, "add",
     ADDR_MASK_IMMEDIATE | ADDR_MASK_DIRECT | ADDR_MASK_MATRIX |
         ADDR_MASK_REGISTER,
     ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},

    {3, "sub",
     ADDR_MASK_IMMEDIATE | ADDR_MASK_DIRECT | ADDR_MASK_MATRIX |
         ADDR_MASK_REGISTER,
     ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},

    {4, "lea", ADDR_MASK_DIRECT | ADDR_MASK_MATRIX,
     ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},

    {5, "clr", 0, ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},
    {6, "not", 0, ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},
    {7, "inc", 0, ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},
    {8, "dec", 0, ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},
    {9, "jmp", 0, ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},
    {10, "bne", 0, ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},
    {11, "jsr", 0, ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},
    {12, "red", 0, ADDR_MASK_DIRECT | ADDR_MASK_MATRIX | ADDR_MASK_REGISTER},

    {13, "prn", 0,
     ADDR_MASK_IMMEDIATE | ADDR_MASK_DIRECT | ADDR_MASK_MATRIX |
         ADDR_MASK_REGISTER},

    {14, "rts", 0, 0},
    {15, "stop", 0, 0},

    {-1, NULL, 0, 0}}; /* mark end of table */

int opcode_from_string(const char *s) {
  int idx;

  if (s == NULL) {
    fprintf(stderr,
            "(ERROR) [first_pass] opcode_from_string failed, received NULL "
            "ptr\n");
    return -1;
  }

  /* search the instruction against our table */
  for (idx = 0; instruction_info_table[idx].name != NULL; ++idx) {
    if (strcmp(s, instruction_info_table[idx].name) == 0) {
      return instruction_info_table[idx].opcode;
    }
  }

  /* not found (unknown instruction) */
  fprintf(stderr,
          "(ERROR) [first_pass] opcode_from_string did not find an opcode for "
          "'%s'\n",
          s);
  return -1;
}

bool is_register(const char *s) {
  if (s == NULL) {
    return false;
  }

  /* must start with 'r' */
  if (s[0] != 'r') {
    return false;
  }

  /* second char must be a digit 0..7 */
  if (s[1] < '0' || s[1] > '7') {
    return false;
  }

  /* must end here (two chars) */
  if (s[2] != '\0') {
    return false;
  }

  return true;
}

/* reg_code -- returns the raw register number (not a bit‑mask)  */
int reg_code(const char *s) {
  /* convert character digit to integer (s[1] is '0'-'7') */
  return s[1] - '0';
}

int addr_mode(const char *operand) {
  if (!operand)
    return -1;

  /* determine addressing mode based on operand syntax */
  if (operand[0] == '#')
    return ADDR_MODE_IMMEDIATE;

  if (is_register(operand))
    return ADDR_MODE_REGISTER;

  if (strchr(operand, '['))
    return ADDR_MODE_MATRIX;

  return ADDR_MODE_DIRECT;
}

bool parse_matrix_regs(const char *op, int *row, int *col) {
  const char *p1, *p2, *p3, *p4;
  char rbuf[32], cbuf[32];

  /* find bracket positions for matrix syntax: label[row][col] */
  p1 = strchr(op, '['); /* first opening bracket */
  if (!p1)
    return false;

  p2 = strchr(p1 + 1, ']'); /* first closing bracket */
  if (!p2)
    return false;

  p3 = strchr(p2 + 1, '['); /* second opening bracket */
  if (!p3)
    return false;

  p4 = strchr(p3 + 1, ']'); /* second closing bracket */
  if (!p4)
    return false;

  /* get rowregister string (between first [ and ]) */
  strncpy(rbuf, p1 + 1, p2 - p1 - 1);
  rbuf[p2 - p1 - 1] = '\0';

  /* get column register string (between second [ and ]) */
  strncpy(cbuf, p3 + 1, p4 - p3 - 1);
  cbuf[p4 - p3 - 1] = '\0';

  /* clean up whitespace from extracted register names */
  trim(rbuf);
  trim(cbuf);

  /* make sure that both are valid register names */
  if (!is_register(rbuf) || !is_register(cbuf))
    return false;

  /* extract register codes for encoding */
  *row = reg_code(rbuf);
  *col = reg_code(cbuf);

  return true;
}

const InstructionInfo *get_instruction_info(int opcode) {
  const InstructionInfo *p = instruction_info_table;

  /* search instruction table for matching opcode */
  while (p->name) {
    if (p->opcode == opcode)
      return p;
    p++;
  }
  return NULL;
}

bool parse_opcode_and_operands(char *line, char **opcode_out,
                               char **operands_out) {
  char *op, *rest;

  if (!line || !opcode_out || !operands_out) {
    return false;
  }

  /* split line at first whitespace, opcode and everything else */
  op = strtok(line, " \t");
  rest = strtok(NULL, "");

  if (!op) {
    /* no opcode found in line */
    *opcode_out = NULL;
    *operands_out = NULL;
    return false;
  }

  *opcode_out = op;

  /* trim operands if present, otherwise set to NULL */
  *operands_out = rest ? trim(rest) : NULL;
  return true;
}

bool parse_two_operands(char *operands, char **src_out, char **dst_out,
                        int line_num, int *err_count) {
  char *comma;

  /* initialize output parameters
   * this is ti avoid errors */
  if (src_out)
    *src_out = NULL;
  if (dst_out)
    *dst_out = NULL;

  /* handle case of no operands */
  if (!operands || !*operands) {
    return true;
  }

  /* look for comma separator between operands */
  comma = strchr(operands, ',');
  if (comma) {
    /* split the first operand */
    *comma = '\0';
    if (src_out) {
      char *s = trim(operands); /* cleanup source operand */

      *src_out = (s && *s) ? s : NULL;
    }
    if (dst_out) {
      char *d = trim(comma + 1); /* cleanup destination operand */

      *dst_out = (d && *d) ? d : NULL;
    }

    /* check for invalid multiple commas */
    if (strchr(comma + 1, ',')) {
      fprintf(stderr, "(ERROR) [first_pass] too many operands at line %d\n",
              line_num);
      (*err_count)++;
      return false;
    }

  } else {
    /* single operand - treat as destination */
    if (dst_out) {
      char *d = trim(operands); /* cleanup */

      *dst_out = (d && *d) ? d : NULL;
    }
  }

  return true;
}

bool check_operand_count(const InstructionInfo *info, const char *src,
                         const char *dst, int line_num, int *err_count) {
  int expected, actual;

  if (!info)
    return false;

  /* calculate expected operand count from instruction constraints
   * some instructions take 0, 1, or 2 operands based on their type
   * allowed_src and allowed_dst flags indicate what the instruction accepts
   * example: mov needs src+dst (2 operands), rts needs neither (0 operands) */
  expected = (info->allowed_src ? 1 : 0) + (info->allowed_dst ? 1 : 0);

  /* count actual operands provided */
  actual = false;
  if (src)
    actual++;

  if (dst)
    actual++;

  /* verify operand count matches instruction requirements */
  if (expected != actual) {
    fprintf(stderr,
            "(ERROR) [first_pass] wrong number of operands at line %d, "
            "expected %d but %d received\n",
            line_num, expected, actual);

    (*err_count)++;

    return false;
  }

  return true;
}

/* validate_operand_modes -- check if operands use legal addressing modes
 * - calculates addressing mode for each operand
 * - verifies mode is allowed for this instruction
 *
 * returns computed modes via output parameters
 */
bool validate_operand_modes(const InstructionInfo *info, const char *src,
                            const char *dst, int *src_mode_out,
                            int *dst_mode_out, int line_num, int *err_count) {
  int computed_src_mode = -1;
  int computed_dst_mode = -1;

  if (!info)
    return false;

  /* validate source operand addressing mode */
  if (src) {
    computed_src_mode = addr_mode(src);

    /* check if source mode is allowed for this instruction */
    /* (info->allowed_src & (1 << mode)) must be nonzero for legal mode */
    if (!(info->allowed_src & (1 << computed_src_mode))) {
      fprintf(stderr,
              "(ERROR) [first_pass] illegal source operand at line %d\n",
              line_num);

      (*err_count)++;
      return false;
    }
  }

  /* validate destination operand addressing mode */
  if (dst) {
    computed_dst_mode = addr_mode(dst);

    /* check if destination mode is allowed for this instruction */
    if (!(info->allowed_dst & (1 << computed_dst_mode))) {
      fprintf(stderr,
              "(ERROR) [first_pass] illegal destination operand at line %d\n",
              line_num);

      (*err_count)++;
      return false;
    }
  }

  /* return computed modes to caller */
  if (src_mode_out)
    *src_mode_out = computed_src_mode;

  if (dst_mode_out)
    *dst_mode_out = computed_dst_mode;

  return true;
}

/* - reg+reg share a single extra word
 * - matrix takes 2 words per operand (label placeholder + index-regs word)
 */
int compute_instruction_length(int src_mode, int dst_mode, const char *src,
                               const char *dst) {
  int L = 1; /* first word always takes at least 1 word */

  if (src && dst && src_mode == ADDR_MODE_REGISTER &&
      dst_mode == ADDR_MODE_REGISTER) {
    /* if both src and dst are registers, both are in 1 word */
    L += 1;
  } else {
    /* tenary operators, if any are matrix - we use (add) 2 words, else 1 */
    if (src) {
      L += (src_mode == ADDR_MODE_MATRIX ? 2 : 1);
    }
    if (dst) {
      L += (dst_mode == ADDR_MODE_MATRIX ? 2 : 1);
    }
  }

  return L;
}

bool validate_immediate_range(int val, int line_num, int *err_count) {
  /* immediate values are limited to 8-bit signed range
   * because only 8 bits are available in the instruction word for immediate
   * payload */
  if (val < MIN_IMMEDIATE_VAL || val > MAX_IMMEDIATE_VAL) {
    fprintf(stderr,
            "(ERROR) [first_pass] immediate value %d out of range (%d to %d) "
            "at line %d\n",
            val, MIN_IMMEDIATE_VAL, MAX_IMMEDIATE_VAL, line_num);

    if (err_count)
      (*err_count)++;

    return false;
  }
  return true;
}
