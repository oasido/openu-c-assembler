#ifndef INSTRUCTION_IMAGE_H

#define INSTRUCTION_IMAGE_H
#include "assembler.h"
#include "types.h"

#define WORD_MASK 0x3FF    /* mask for a 10-bit word */
#define NIBBLE_MASK 0xF    /* mask for a 4-bit register id */
#define IMM_MASK 0xFF      /* mask for 8-bit immediate payload (bits 2..9) */
#define ADDR_MODE_MASK 0x3 /* mask for 2-bit src/dst field(s) */
#define ARE_MASK 0x3       /* mask for 2-bit A/R/E field */
#define IMM_DATA_SHIFT 2   /* shift for 8-bit imm data to skip ARE bits */
#define REG_DST_SHIFT 2    /* shift for destination register field (bits 2-5) */
#define REG_SRC_SHIFT 6    /* shift for source register field (bits 6-9) */

extern int instruction_image[MAX_WORDS_MEMORY];
extern CommandFields *command_list[MAX_WORDS_MEMORY];
extern int command_count;

/* append_command -- push a parsed command into the pending list
 *
 * behavior:
 * - on success: stores cf at the next slot and bumps command_count
 * - on overflow: prints error and frees cf to avoid leaks
 */
void append_command(CommandFields *cf);

/* record_command -- allocate + append a CommandFields entry (returns the node
 * or NULL) */
CommandFields *record_command(int start_ic, int length_words, int opcode,
                              const char *src, const char *dst,
                              const char *label_or_null);

/* emit_word -- write one 10-bit code word into the instruction image at IC and
 * increase IC
 *
 * returns on success the index written (index relative to instruction_image)
 * and -1 if IC points outside the allowed code image range
 */
int emit_word(int value, int *IC);

/* emit_first_word -- ("builds the first word") pack opcode+addr modes into the
 * first word and emit uses OPCODE_MASK/SHIFT etc.
 *
 * [9..6]=opcode, [5..4]=src mode, [3..2]=dst mode, [1..0]=A/R/E
 *
 * returns 1 on success, 0 on failure
 */
int emit_first_word(int opcode, int src_mode, int dst_mode, int *IC);

/* emit_operands -- emit operand words according to addressing modes
 *
 * - register+register: one word, src in high reg field, dst in low reg field
 * - REGISTER alone: one word with its field set
 * - IMMEDIATE (#n): store signed 8-bit in bits [9..2], A/R/E [1..0] left 0
 * - DIRECT: emit 0 placeholder (will be resolved in pass 2)
 * - MATRIX: emit 0 placeholder word for label, then one word packing the two
 * index regs
 *
 * returns true on success, false if any issue was detected (still tries to
 * continue)
 */
bool emit_operands(const char *src, int src_mode, const char *dst, int dst_mode,
                   int *IC, int line_num, int *err_count);

/* new_command -- allocate and zero a CommandFields record
 *
 * - MUST BE FREED!
 */
CommandFields *new_command(void);

/* free_command -- release one CommandFields (owned strings + struct)
 *
 * notes:
 * - safe to call with NULL.
 * - frees label, src, dst if present, then the struct.
 */
void free_command(CommandFields *cf);

/* free_commands -- release all stored CommandFields and reset the list
 *
 * - iterates over command_list[0..command_count-1] and calls free_command on
 * each
 */
void free_commands(void);

#endif /* INSTRUCTION_IMAGE_H */
