#ifndef INSTRUCTION_UTILS_H
#define INSTRUCTION_UTILS_H

#include "instruction_image.h"
#include "types.h"

/* addr modes */
#define ADDR_MODE_IMMEDIATE 0
#define ADDR_MODE_DIRECT 1
#define ADDR_MODE_MATRIX 2
#define ADDR_MODE_REGISTER 3

/* addr masks - bitmasks for checking which addressing modes an instruction
 * supports */
#define ADDR_MASK_IMMEDIATE (1 << ADDR_MODE_IMMEDIATE)
#define ADDR_MASK_DIRECT (1 << ADDR_MODE_DIRECT)
#define ADDR_MASK_MATRIX (1 << ADDR_MODE_MATRIX)
#define ADDR_MASK_REGISTER (1 << ADDR_MODE_REGISTER)

/* below are defines for static encoding helpers */

/* bit positions (lsb = 0) - shifts for fields we pack into extra words */
#define DST_MODE_SHIFT 2
#define SRC_MODE_SHIFT 4
#define OPCODE_SHIFT 6

/* mask for extracting opcode field (4 bits) */
#define OPCODE_MASK NIBBLE_MASK

typedef struct InstructionInfo {
  int opcode;
  const char *name;
  int allowed_src;
  int allowed_dst;
} InstructionInfo;

/* opcode_from_string -- returns opcode number for instruction name */
int opcode_from_string(const char *s);

/* is_register -- checks if string is valid register format (r0-r7) */
bool is_register(const char *s);

/* reg_code -- extracts register number from register string */
int reg_code(const char *s);

/* addr_mode -- determines addressing mode from operand syntax */
int addr_mode(const char *operand);

/* parse_matrix_regs -- extracts row and column registers from matrix syntax */
bool parse_matrix_regs(const char *op, int *row, int *col);

extern const InstructionInfo instruction_info_table[];

/* get_instruction_info -- lookup instruction "data" by opcode
 *
 * returns NULL if not found */
const InstructionInfo *get_instruction_info(int opcode);

/* parse_opcode_and_operands -- split a line into opcode and the rest
 * (operands)
 *
 * we modify 'line' via strtok and output pointers into that same
 * buffer
 *
 * returns true on success (opcode found), false on error
 */
bool parse_opcode_and_operands(char *line, char **opcode_out,
                               char **operands_out);

/* parse_two_operands -- split a operands string (possibly NULL/empty) into
 * src,dst
 * - requires at most one comma
 * - empty strings become NULL
 *
 * returns true on success, false on error (and bumps *err_count ..)
 */
bool parse_two_operands(char *operands, char **src_out, char **dst_out,
                        int line_num, int *err_count);

/* check_operand_count -- verify count matches the opcode's expectation
 * expected comes from info->allowed_src/allowed_dst flags
 * we convert that to 0/1 and compare with actual (src!=NULL, dst!=NULL).
 *
 * returns 1 (true) if ok, 0 if mismatch (prints error)
 */
bool check_operand_count(const InstructionInfo *info, const char *src,
                         const char *dst, int line_num, int *err_count);

/* validate_operand_modes -- calculate "modes" and check legality vs opcode
 * out params get -1 if operand is NULL
 *
 * returns true if ok, false on error
 */
bool validate_operand_modes(const InstructionInfo *info, const char *src,
                            const char *dst, int *src_mode_out,
                            int *dst_mode_out, int line_num, int *err_count);

/* compute_instruction_length -- words needed for this instruction
 * base word is 1 - reg+reg shares one word; matrix uses 2 words per operand */
int compute_instruction_length(int src_mode, int dst_mode, const char *src,
                               const char *dst);

/* validate_immediate_range -- checks if value fits in 8-bit signed immediate
 *
 * returns true if valid, false if out of range */
bool validate_immediate_range(int val, int line_num, int *err_count);

#endif /* INSTRUCTION_UTILS_H */
