// Copyright (c), 2023-2025, Armin Biere, University of Freiburg.

// This is a tool to normalize CNFs in DIMACS format by removing all
// comments and white-space (it also checks for syntax issues).

// clang-format off

static const char * usage =
"usage: normalize [ -h ] [ <input> [ <output> ] ]\n"
"\n"
"  -h | --help  print this comand line option summary\n"
"  -g | --gbd   GBD normalize (no 'p' line, strip last '\\n', '\\n' -> ' ')\n"
"  <input>      input file expected to be in DIMACS format\n"
"  <output>     output file produced in DIMACS format\n"
"\n"
"The file arguments can be '-' to denote '<stdin>' respectively '<stdout>\n"
"which are also the default files if not specified.  If further the path\n"
"of a file has a '.xz' suffix, it is decompressed respectively compressed\n"
"using 'xz' on-the-fly (through a pipe).\n"
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
#include <sys/stat.h>

static const char *input_path, *output_path;
static FILE *input_file, *output_file;
static int close_input, close_output;
static bool gbd;

static void die(const char *fmt, ...) {
  fprintf(stderr, "normalize: error in '%s': ", input_path);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static bool has_suffix(const char *a, const char *b) {
  size_t k = strlen(a), l = strlen(b);
  return k >= l && !strcmp(a + k - l, b);
}

static bool exists_file(const char *path) {
  struct stat buf;
  return !stat(path, &buf);
}

int main(int argc, char **argv) {
  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      fputs(usage, stdout);
      exit(0);
    } else if (!strcmp(arg, "-g") || !strcmp(arg, "--gbd"))
      gbd = true;
    else if (output_path)
      die("too many files");
    else if (input_path)
      output_path = arg;
    else
      input_path = arg;
  }
  if (!input_path || !strcmp(input_path, "-")) {
    input_file = stdin;
    input_path = "<stdin>";
    close_input = 0;
  } else if (!exists_file(input_path))
    die("input file '%s' does not exist", input_path);
  else if (has_suffix(input_path, ".xz")) {
    size_t len = strlen(input_path) + 16;
    char *cmd = malloc(len);
    snprintf(cmd, len, "xz -d -c %s", input_path);
    input_file = popen(cmd, "r");
    free(cmd);
    close_input = 2;
  } else {
    input_file = fopen(input_path, "r");
    close_input = 1;
  }
  if (!input_file)
    die("can not read input file '%s'", input_path);
  int ch;
  for (;;) {
    ch = getc(input_file);
    if (ch == 'c') {
      while ((ch = getc(input_file)) != '\n')
        if (ch == EOF)
        END_OF_FILE_IN_COMMENT:
          die("end-of-file in comment");
    } else if (ch == ' ' || ch == '\t' || ch == '\r') {
      while ((ch = getc(input_file)) != '\n')
        if (ch == EOF)
          die("unexpected end-of-file after white-space");
    } else if (ch != '\n')
      break;
  }
  if (ch != 'p')
    die("expected 'p cnf ...' header or 'c' comment");
  for (const char *p = " cnf "; *p; p++)
    if (*p != getc(input_file))
      die("invalid 'p cnf ...' header");
  ch = getc(input_file);
  if (!isdigit(ch))
  INVALID_VARIABLES:
    die("invalid number of variables");
  int variables = ch - '0';
  while (isdigit(ch = getc(input_file))) {
    if (INT_MAX / 10 < variables)
      goto INVALID_VARIABLES;
    variables *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < variables)
      goto INVALID_VARIABLES;
    variables += digit;
  }
  if (ch != ' ')
    die("expected space in header after variables");
  ch = getc(input_file);
  if (!isdigit(ch))
  INVALID_CLAUSES:
    die("invalid number of clauses");
  int clauses = ch - '0';
  while (isdigit(ch = getc(input_file))) {
    if (INT_MAX / 10 < clauses)
      goto INVALID_CLAUSES;
    clauses *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < clauses)
      goto INVALID_CLAUSES;
    clauses += digit;
  }
  if (ch == '\r')
    ch = getc(input_file);
  if (ch == ' ' || ch == '\t') {
    while ((ch = getc(input_file)) != '\n')
      if (ch != ' ' && ch != '\t' && ch != '\r')
      EXPECTED_NEW_LINE_AFTER_HEADER:
        die("expected white-space and a new-line after clauses");
  } else if (ch != '\n')
    goto EXPECTED_NEW_LINE_AFTER_HEADER;
  if (!output_path || !strcmp(output_path, "-")) {
    output_file = stdout;
    close_output = 0;
  } else if (has_suffix(output_path, ".xz")) {
    size_t len = strlen(output_path) + 16;
    char *cmd = malloc(len);
    snprintf(cmd, len, "xz -e -c > %s", output_path);
    output_file = popen(cmd, "w");
    free(cmd);
    close_output = 2;
  } else {
    output_file = fopen(output_path, "w");
    close_output = 1;
  }
  if (!output_file)
    die("can not write output file '%s'", output_path);
  if (!gbd)
    fprintf(output_file, "p cnf %d %d\n", variables, clauses);
  int parsed = 0, lit = 0;
  bool first = true;
  for (;;) {
    ch = getc(input_file);
    if (ch == EOF) {
      if (lit)
        die("zero at end of last clause missing");
      if (parsed < clauses)
        die("clause missing");
      break;
    }
    if (ch == 'c') {
      while ((ch = getc(input_file)) != '\n')
        if (ch == EOF) {
          if (lit || parsed < clauses)
            goto END_OF_FILE_IN_COMMENT;
          else
            break;
        }
      continue;
    }
    if (ch == '\r')
      ch = getc(input_file);
    if (ch == ' ' || ch == '\n' || ch == '\t')
      continue;
    int sign = 1;
    if (ch == '-') {
      ch = getc(input_file);
      sign = -1;
    }
    if (!isdigit(ch))
    INVALID_LITERAL:
      die("invalid literal");
    if (gbd && !lit) {
      if (first)
        first = false;
      else
        fputc(' ', output_file);
    }
    lit = ch - '0';
    while (isdigit(ch = getc(input_file))) {
      if (INT_MAX / 10 < lit)
        goto INVALID_LITERAL;
      lit *= 10;
      int digit = ch - '0';
      if (INT_MAX - digit < lit)
        goto INVALID_LITERAL;
      lit += digit;
    }
    if (lit > variables)
      goto INVALID_LITERAL;
    if (ch == '\r')
      ch = getc(input_file);
    if (ch != ' ' && ch != '\n' && ch != '\t' && ch != 'c' && ch != EOF)
      die("expected white-space after literal");
    if (ch == 'c') {
      while ((ch = getc(input_file)) != '\n')
        if (ch == EOF) {
          if (lit || parsed < clauses)
            goto END_OF_FILE_IN_COMMENT;
          else
            break;
        }
    }
    if (lit)
      fprintf(output_file, "%d ", sign * lit);
    else if (parsed++ == clauses)
      die("too many clauses");
    else if (gbd)
      fputc('0', output_file);
    else
      fputs("0\n", output_file);
  }
  if (close_input == 1)
    fclose(input_file);
  if (close_input == 2)
    pclose(input_file);
  if (close_output)
    fclose(output_file);
  return 0;
}
