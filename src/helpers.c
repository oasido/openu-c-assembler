#include "helpers.h"
#include "assembler.h"
#include "instruction_image.h"
#include "types.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* helpers -- utility functions for memory, file, string, and data processing */

char *safe_strdup(const char *s) {
  char *dup = (char *)malloc(strlen(s) + 1);

  if (!dup) {
    /* strlen returns unsigned long so we cast */
    fprintf(stderr, "(ERROR) [helpers] malloc failed allocating %lu bytes\n",
            (unsigned long)(strlen(s) + 1));
    return NULL;
  }

  strcpy(dup, s);

  return dup;
}

void *safe_calloc(size_t num, size_t size) {
  void *void_ptr = calloc(num, size);

  if (void_ptr == NULL) {
    fprintf(stderr, "(ERROR) [memory] calloc failed allocating %lu bytes\n",
            (unsigned long)num * size);
  }

  return void_ptr;
}

/* to_twos_complement -- converts negative number to positive two's complement
 * form
 *
 * returns the positive equivalent for two's complement representation
 */
static unsigned int to_twos_complement(int value) {
  /* for negative numbers, two's complement is: flip all bits and add 1
   * example: -5 becomes ~(-5) + 1 = ~(11111011) + 1 = 00000100 + 1 = 00000101
   */
  return (unsigned int)(~value + 1);
}

/* decimal_to_binary -- convert signed integer into two's-complement binary
 * representation of length 'bits'; returns a malloc'd null-terminated string.
 */
char *decimal_to_binary(int value, int bits) {
  char *buf;
  unsigned int working_value;
  int idx;

  if (bits <= 0) {
    fprintf(stderr, "(ERROR) [helpers] number of bits must be positive\n");
    return NULL;
  }

  /* number of bits including null */
  buf = safe_calloc(bits + 1, sizeof(*buf));
  if (!buf) {
    fprintf(stderr,
            "(ERROR) [helpers] safe_calloc failed in decimal_to_binary\n");
    return NULL;
  }

  /* handle two's complement - negative numbers get converted */
  if (value < 0) {
    working_value = to_twos_complement(value);
  } else {
    working_value = (unsigned int)value;
  }

  /* convert to binary string by dividing by 2
   * we build string from right to left (LSB first) */
  for (idx = bits - 1; idx >= 0; idx--) {
    buf[idx] = (working_value % 2) ? '1' : '0';
    working_value = working_value / 2;
  }

  buf[bits] = '\0'; /* terminate string */
  return buf;
}

FILE *open_file_with_ext(const char *base, const char *ext, const char *mode) {
  char filename[MAX_FILENAME_LENGTH + EXT_LENGTH];
  FILE *temp;

  if (strlen(base) + strlen(ext) >= sizeof(filename)) {
    /* name too long */
    fprintf(stderr,
            "(ERROR) [helpers] open_file_with_ext failed (%s.%s, mode: %s)\n "
            "filename too long\n",
            base, ext, mode);
    return NULL;
  }
  strcpy(filename, base);
  strcat(filename, ext);

  temp = fopen(filename, mode);

  if (!temp) {
    fprintf(stderr,
            "(INFO) [helpers] open_file_with_ext failed (%s.%s, mode: %s)\n",
            base, ext, mode);
  }

  /* return the file pointer or NULL */
  return temp;
}

/* usage: close_files(f1, f2, NULL) */
/* we still use fclose throughout the project if we're closing a single file :)
 */
int close_files(FILE *first, ...) {
  va_list args;
  FILE *fp = first;
  int closed = 0;

  va_start(args, first);
  while (fp != NULL) {
    if (fclose(fp) == 0) {
      closed++;
    } else {
      fprintf(stderr, "(ERROR) [file] fclose failed\n");
    }
    fp = va_arg(args, FILE *);
  }

  va_end(args);
  return closed;
}

void remove_comment(char *line) {
  /* chop the line at ';' if present */
  char *semicolon = strchr(line, ';');
  if (semicolon) {
    *semicolon = '\0';
  }
}

char *trim_left(char *str) {
  char *start = str; /* start of the data */

  /* skip past any leading whitespace */
  while (*start && isspace((unsigned char)*start)) {
    start++;
  }

  /* move the trimmed data to the original pointer */
  /* (instead of returning a new pointer) */
  if (start != str) {
    /* shift the content left in place */
    memmove(str, start, strlen(start) + 1);
  }

  return str;
}

char *trim_right(char *str) {
  char *end = str + strlen(str); /* point to the null terminator */

  if (end == str)
    return str;

  /* walk backwards over trailing spaces */
  while (end > str && isspace((unsigned char)*(end - 1))) {
    end--;
  }

  *end = '\0';

  return str;
}

char *trim_inbetween(char *str) {
  char tmp[MAX_LINE_LENGTH];
  char *read = str;
  char *write = tmp;
  bool saw_space = false;
  bool in_string = false;
  char quote_char = '\0';

  while (*read) {
    if (in_string) {
      /* if we're inside quotation marks */
      *write++ = *read;
      if (*read == quote_char) {
        /* end quotation mark */
        in_string = false;
      }
    } else if (*read == ':') {
      if (!saw_space) {
        *write++ = *read;
      }
      *write++ = ' ';
      saw_space = true;
    } else if (*read == ',' && saw_space) {
      if (saw_space) {
        write--;
      }
      *write++ = ',';
      *write++ = ' ';
      saw_space = true;
    } else if (*read == '"' || *read == '\'') {
      in_string = true;
      quote_char = *read;
      *write++ = *read;
      saw_space = false;
    } else if (isspace((unsigned char)*read)) {
      if (!saw_space) {
        *write++ = ' ';
        saw_space = true;
      }
    } else {
      *write++ = *read;
      saw_space = false;
    }
    read++;
  }

  *write = '\0';
  strcpy(str, tmp);
  return str;
}

char *trim(char *str) {
  trim_left(str);
  trim_right(str);
  trim_inbetween(str);
  return str;
}

char *cleanup_line(char *line) {
  remove_comment(line);
  return trim(line);
}

FILE *cleanup_file(FILE *in, FILE *out) {
  char line[MAX_LINE_LENGTH];
  /* process each line and write the cleaned result */
  while (fgets(line, sizeof(line), in)) {
    cleanup_line(line);
    if (line[0] == '\0')
      continue;
    fprintf(out, "%s\n", line);
  }
  return out;
}

void check_trailing_comma(char *s, int line_number, int *error_count) {
  char *end;

  /* nothing to check on null or empty string */
  if (!s || *s == '\0')
    return;

  end = s + strlen(s) - 1;

  /* traverse backwards over trailing whitespace */
  while (end >= s && isspace((unsigned char)*end))
    end--;

  if (end >= s && *end == ',') {
    fprintf(stderr, "(ERROR) [first_pass] trailing comma at line %d\n",
            line_number);
    (*error_count)++;
  }
}

bool is_illegal_name(char *name) {
  /* list of reserved words that cannot be macro/label names */
  const char *ILLEGAL[] = {"mcro",   "mcroend", "mov",     "add",
                           "sub",    ".data",   ".string", ".extern",
                           ".entry", "stop",    NULL};
  int idx = 0;
  /* compare against each keyword */
  while (ILLEGAL[idx]) {
    if (strcmp(name, ILLEGAL[idx]) == 0)
      return true;
    idx++;
  }
  return false;
}

bool is_valid_data_num(const char *str) {
  int length = strlen(str), idx = 0;

  if (str == NULL || str[0] == '\0')
    return false;

  /* if the number is being prepended with +/- */
  if ((str[idx] == '+') || (str[idx] == '-')) {
    idx++;

    /* invalid, +/- must be followed by a digit */
    if (str[idx] == '\0')
      return false;
  }

  while (idx < length) {
    if (isdigit(str[idx]) == false) {
      return false;
    }
    idx++;
  }

  return true;
}

bool is_num_within_range(short num) {
  /* 10 bit max signed values, defined in assembler.h */
  return !(num < MIN_WORD_VAL || num > MAX_WORD_VAL);
}

char *decimal_to_base4_letters(int decimal_value) {
  char *result;
  int idx;
  int temp_value;

  /* allocate space for 5 letters + null terminator */
  result = safe_calloc(BASE4_STRING_LENGTH, sizeof(char));
  if (!result) {
    return NULL;
  }

  /* make sure we only work with 10-bit values
   * we use AND with the WORD_MASK to zero out bits that we don't use */
  temp_value = decimal_value & WORD_MASK;

  /* convert to base 4 from right to left */
  /* we fill the string backwards, starting from position 4 */
  for (idx = 4; idx >= 0; idx--) {
    int digit;

    /* get the rightmost base-4 digit by remainder */
    digit = temp_value % 4;

    /* convert digit to letter */
    if (digit == 0) {
      result[idx] = 'a';
    } else if (digit == 1) {
      result[idx] = 'b';
    } else if (digit == 2) {
      result[idx] = 'c';
    } else {
      result[idx] = 'd';
    }

    /* divide by 4 to get the next digit */
    temp_value = temp_value / 4;
  }

  /* add null terminator */
  result[5] = '\0';

  return result;
}
