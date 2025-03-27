// Copyright (c), 2023, Armin Biere, University of Freiburg.

// This is a tool to normalize CNFs in DIMACS format by removing all
// comments and white-space (it also checks for syntax issues).

// clang-format off

static const char * usage =
"usage: normalize [ -h ] [ <input> [ <output> ] ]\n"
"\n"
"  -h        print this comand line option summary\n"
"  <input>   input file expected to be in DIMACS format\n"
"  <output>  output file produced in DIMACS format\n"
"\n"
"The file arguments can be '-' to denote '<stdin>' respectively '<stdout>\n"
"which are also the default files if not specified.\n"
;

// clang-format on

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *input_path, *output_path;
static FILE *input_file, *output_file;
static bool close_input, close_output;

static void die(const char* fmt, ...) {
  fprintf(stderr, "normalize: error in '%s': ", input_path);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

int main(int argc, char** argv) {
  for (int i = 1; i != argc; i++) {
    const char* arg = argv[i];
    if (!strcmp(arg, "-h")) {
      fputs(usage, stdout);
      exit(0);
    } else if (output_path)
      die("too many files");
    else if (input_path)
      output_path = arg;
    else
      input_path = arg;
  }
  if (!input_path || !strcmp(input_path, "-"))
    input_file = stdin, assert(!close_input), input_path = "<stdin>";
  else if (!(input_file = fopen(input_path, "r")))
    die("can not read input file '%s'", input_path);
  else
    close_input = true;
  int ch;
  while ((ch = getc(input_file)) == 'c')
    while ((ch = getc(input_file)) != '\n')
      if (ch == EOF)
      END_OF_FILE_IN_COMMENT:
	die("end-of-file in comment");
  if (ch != 'p') die("expected 'p cnf ...' header or 'c' comment");
  for (const char* p = " cnf "; *p; p++)
    if (*p != getc(input_file)) die("invalid 'p cnf ...' header");
  ch = getc(input_file);
  if (!isdigit(ch))
  INVALID_VARIABLES:
    die("invalid number of variables");
  int variables = ch - '0';
  while (isdigit(ch = getc(input_file))) {
    if (INT_MAX / 10 < variables) goto INVALID_VARIABLES;
    variables *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < variables) goto INVALID_VARIABLES;
    variables += digit;
  }
  if (ch != ' ') die("expected space in header after variables");
  ch = getc(input_file);
  if (!isdigit(ch))
  INVALID_CLAUSES:
    die("invalid number of clauses");
  int clauses = ch - '0';
  while (isdigit(ch = getc(input_file))) {
    if (INT_MAX / 10 < clauses) goto INVALID_CLAUSES;
    clauses *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < clauses) goto INVALID_CLAUSES;
    clauses += digit;
  }
  if (ch == '\r') ch = getc(input_file);
  if (ch == ' ' || ch == '\t') {
    while ((ch = getc(input_file)) != '\n')
      if (ch != ' ' && ch != '\t' && ch != '\r')
      EXPECTED_NEW_LINE_AFTER_HEADER:
	die("expected white-space and a new-line after clauses");
  } else if (ch != '\n')
    goto EXPECTED_NEW_LINE_AFTER_HEADER;
  if (!output_path || !strcmp(output_path, "-"))
    output_file = stdout, assert(!close_output);
  else if (!(output_file = fopen(output_path, "w")))
    die("can not write output file '%s'", output_path);
  else
    close_input = true;
  fprintf(output_file, "p cnf %d %d\n", variables, clauses);
  int parsed = 0, lit = 0;
  for (;;) {
    ch = getc(input_file);
    if (ch == EOF) {
      if (lit) die("zero at end of last clause missing");
      if (parsed < clauses) die("clause missing");
      break;
    }
    if (ch == 'c') {
      while ((ch = getc(input_file)) != '\n')
	if (ch == EOF) goto END_OF_FILE_IN_COMMENT;
      continue;
    }
    if (ch == '\r') ch = getc(input_file);
    if (ch == ' ' || ch == '\n' || ch == '\t') continue;
    int sign = 1;
    if (ch == '-') {
      ch = getc(input_file);
      sign = -1;
    }
    if (!isdigit(ch))
    INVALID_LITERAL:
      die("invalid literal");
    lit = ch - '0';
    while (isdigit(ch = getc(input_file))) {
      if (INT_MAX / 10 < lit) goto INVALID_LITERAL;
      lit *= 10;
      int digit = ch - '0';
      if (INT_MAX - digit < lit) goto INVALID_LITERAL;
      lit += digit;
    }
    if (lit > variables) goto INVALID_LITERAL;
    if (ch == '\r') ch = getc(input_file);
    if (ch != ' ' && ch != '\n' && ch != '\t' && ch != 'c' && ch != EOF)
      die("expected white-space after literal");
    if (ch == 'c') {
      while ((ch = getc(input_file)) != '\n')
	if (ch == EOF) goto END_OF_FILE_IN_COMMENT;
    }
    if (lit)
      fprintf(output_file, "%d ", sign * lit);
    else if (parsed++ == clauses)
      die("too many clauses");
    else
      fputs("0\n", output_file);
  }
  if (close_input) fclose(input_file);
  if (close_output) fclose(output_file);
  return 0;
}
