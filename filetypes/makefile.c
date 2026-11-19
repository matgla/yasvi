/*
 Copyright (c) 2026 Mateusz Stadnik

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "makefile.h"

#include <string.h>

#include "shell.h"

static const char* make_extensions[] = {".mk", ".mak", NULL};
static const char* make_filenames[] = {"Makefile",    "makefile",    "GNUmakefile",
                                       "Makefile.in", "Makefile.am", NULL};

static const char* make_directives[] = {
  "include", "-include", "sinclude", "ifeq",    "ifneq",    "ifdef",
  "ifndef",  "else",     "endif",    "define",  "endef",    "export",
  "unexport", "override", "private", "vpath",   "undefine", NULL};

// Directives followed by another statement ("export CC = gcc",
// "else ifeq (...)").
static const char* make_modifiers[] = {"export", "unexport", "override", "private",
                                       "else",   NULL};

static const char* make_functions[] = {
  "subst",    "patsubst", "strip",     "findstring", "filter",   "filter-out",
  "sort",     "word",     "wordlist",  "words",      "firstword", "lastword",
  "dir",      "notdir",   "suffix",    "basename",   "addsuffix", "addprefix",
  "join",     "wildcard", "realpath",  "abspath",    "error",    "warning",
  "info",     "shell",    "origin",    "flavor",     "foreach",  "if",
  "or",       "and",      "call",      "eval",       "file",     "value",
  "let",      "intcmp",   "guile",     NULL};

static const char* make_special_targets[] = {
  ".PHONY",       ".SUFFIXES",  ".DEFAULT",          ".PRECIOUS",
  ".INTERMEDIATE", ".NOTINTERMEDIATE", ".SECONDARY", ".SECONDEXPANSION",
  ".DELETE_ON_ERROR", ".IGNORE", ".LOW_RESOLUTION_TIME", ".SILENT",
  ".EXPORT_ALL_VARIABLES", ".NOTPARALLEL", ".ONESHELL", ".POSIX",
  NULL};

static const char* make_variables[] = {
  "CC",    "CXX",    "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "LDLIBS", "AR",
  "ARFLAGS", "AS",   "MAKE",   "MAKEFLAGS", "SHELL",   "PREFIX",  "DESTDIR", "RM",
  "VPATH", "CURDIR", "MAKECMDGOALS", ".DEFAULT_GOAL", ".RECIPEPREFIX", NULL};

static const char** completion_words[] = {make_directives, make_functions,
                                          make_special_targets, make_variables, NULL};

// lang_state: what the next line continues.
enum {
  MakeState_Normal = 0,
  MakeState_Value = 1,    // a backslash-continued assignment, rule or directive
  MakeState_Recipe = 2,   // a backslash-continued recipe line
  MakeState_Comment = 3,  // a backslash-continued comment
  MakeState_Define = 16,  // inside define ... endef; 16 + nesting depth - 1
};

static bool make_is_space(char c) {
  return c == ' ' || c == '\t';
}

static bool make_is_word_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         c == '_' || c == '-' || c == '.';
}

static bool make_is_one_of(const char** words, const char* text, int length) {
  for (const char** word = words; *word != NULL; ++word) {
    if ((int)strlen(*word) == length && strncmp(*word, text, (size_t)length) == 0) {
      return true;
    }
  }
  return false;
}

static void make_mark(HighlightContext* ctx, int start, int end, EHighlightToken token) {
  for (int i = start; i < end && i < ctx->len; ++i) {
    ctx->tokens[i] = (char)token;
  }
}

static int make_skip_space(const char* line, int i, int length) {
  while (i < length && make_is_space(line[i])) {
    ++i;
  }
  return i;
}

// The end of the word at `i`: up to white space.
static int make_word_end(const char* line, int i, int length) {
  while (i < length && !make_is_space(line[i])) {
    ++i;
  }
  return i;
}

// Whether the line ends in a backslash that is not itself escaped.
static bool make_continues(const char* line, int length) {
  int count = 0;
  while (count < length && line[length - 1 - count] == '\\') {
    ++count;
  }
  return count % 2 == 1;
}

// The end of the reference starting with the '$' at `i`, without marking it.
static int make_ref_end(const char* line, int i, int length) {
  if (i + 1 >= length) {
    return i + 1;
  }
  const char open = line[i + 1];
  if (open != '(' && open != '{') {
    return i + 2;
  }
  const char close = open == '(' ? ')' : '}';
  int depth = 0;
  for (int k = i + 2; k < length; ++k) {
    if (line[k] == open) {
      ++depth;
    } else if (line[k] == close) {
      if (depth == 0) {
        return k + 1;
      }
      --depth;
    }
  }
  return length;
}

// Marks the reference starting with the '$' at `i`; returns where it ends.
// $(VAR) ${VAR} $(VAR:.c=.o) $@ $X as variables, $(func args) with the
// function name as a keyword and references in its arguments, $$ untouched.
static int make_ref(HighlightContext* ctx, int i, int length) {
  const char* line = ctx->line;
  if (i + 1 >= length) {
    return i + 1;
  }
  const char open = line[i + 1];
  if (open == '$') {
    return i + 2;  // an escaped dollar, for the shell
  }
  if (open != '(' && open != '{') {
    make_mark(ctx, i, i + 2, EHighlightToken_Preprocessor);
    return i + 2;
  }
  const char close = open == '(' ? ')' : '}';
  int name_end = i + 2;
  while (name_end < length && make_is_word_char(line[name_end])) {
    ++name_end;
  }
  if (name_end < length && make_is_space(line[name_end]) &&
      make_is_one_of(make_functions, line + i + 2, name_end - i - 2)) {
    make_mark(ctx, i, i + 2, EHighlightToken_Preprocessor);
    make_mark(ctx, i + 2, name_end, EHighlightToken_Keyword);
    int depth = 0;
    int k = name_end;
    while (k < length) {
      if (line[k] == '$') {
        k = make_ref(ctx, k, length);
        continue;
      }
      if (line[k] == open) {
        ++depth;
      } else if (line[k] == close) {
        if (depth == 0) {
          make_mark(ctx, k, k + 1, EHighlightToken_Preprocessor);
          return k + 1;
        }
        --depth;
      } else if (line[k] == ',') {
        make_mark(ctx, k, k + 1, EHighlightToken_Symbol);
      }
      ++k;
    }
    return length;
  }
  const int end = make_ref_end(line, i, length);
  make_mark(ctx, i, end, EHighlightToken_Preprocessor);
  return end;
}

// Text where only references and a # comment mean something: values,
// prerequisites, directive arguments. `strings` marks quoted strings too
// (ifeq "a" "b").
static void make_value(HighlightContext* ctx, int i, int length, bool strings) {
  const char* line = ctx->line;
  while (i < length) {
    const char c = line[i];
    if (c == '$') {
      i = make_ref(ctx, i, length);
    } else if (c == '\\' && i + 1 < length) {
      i += 2;  // \# is not a comment
    } else if (c == '#') {
      make_mark(ctx, i, length, EHighlightToken_Comment);
      return;
    } else if (strings && (c == '"' || c == '\'')) {
      int end = i + 1;
      while (end < length && line[end] != c) {
        ++end;
      }
      end = end < length ? end + 1 : length;
      make_mark(ctx, i, end, EHighlightToken_String);
      i = end;
    } else {
      ++i;
    }
  }
}

// A recipe from `start`: @ - + prefixes as symbols, then the shell, then
// make's references over it. A `continued` line carries the shell's open
// quote in ctx->in_string / string_char, which are left for the next line.
static void make_recipe(HighlightContext* ctx, int start, bool continued) {
  const char* line = ctx->line;
  int i = start;
  if (!continued) {
    i = make_skip_space(line, i, ctx->len);
    while (i < ctx->len && (line[i] == '@' || line[i] == '-' || line[i] == '+')) {
      ctx->tokens[i++] = (char)EHighlightToken_Symbol;
    }
  }
  HighlightContext shell = {
    .line = line + i,
    .len = ctx->len - i,
    .tokens = ctx->tokens + i,
    .in_string = continued && ctx->in_string,
    .string_char = continued ? ctx->string_char : 0,
  };
  if (shell.len > 0) {
    g_filetype_shell.highlighter->highlight_line(&shell);
  }
  ctx->in_string = shell.in_string;
  ctx->string_char = shell.in_string ? shell.string_char : 0;
  for (int k = i; k < ctx->len;) {
    if (line[k] == '$') {
      k = make_ref(ctx, k, ctx->len);
    } else {
      ++k;
    }
  }
}

typedef enum {
  MakeStatement_Other,
  MakeStatement_Assignment,
  MakeStatement_Rule,
} MakeStatement;

// Classifies the statement in [i, length): an assignment (`*op_start` and
// `*op_end` around its operator), a rule (`*op_start`/`*op_end` around the
// : or ::), or anything else. References are skipped, a # ends it.
static MakeStatement make_classify(const char* line,
                                   int i,
                                   int length,
                                   int* op_start,
                                   int* op_end) {
  for (int k = i; k < length; ++k) {
    const char c = line[k];
    if (c == '$') {
      k = make_ref_end(line, k, length) - 1;
    } else if (c == '#') {
      break;
    } else if (c == '=') {
      *op_start = k > i && (line[k - 1] == '?' || line[k - 1] == '+' || line[k - 1] == '!')
                    ? k - 1
                    : k;
      *op_end = k + 1;
      return MakeStatement_Assignment;
    } else if (c == ':') {
      int end = k + 1;
      if (end < length && line[end] == ':') {
        ++end;
      }
      if (end < length && line[end] == ':') {
        ++end;  // :::= is an assignment
      }
      *op_start = k;
      if (end < length && line[end] == '=') {
        *op_end = end + 1;
        return MakeStatement_Assignment;
      }
      *op_end = end;
      return MakeStatement_Rule;
    }
  }
  return MakeStatement_Other;
}

// The words of a rule's targets: special targets as keywords, others as
// Keyword2, % as a pattern, references.
static void make_targets(HighlightContext* ctx, int i, int end) {
  const char* line = ctx->line;
  while (i < end) {
    i = make_skip_space(line, i, end);
    int word_end = i;
    while (word_end < end && !make_is_space(line[word_end])) {
      if (line[word_end] == '$') {
        word_end = make_ref_end(line, word_end, end);
      } else {
        ++word_end;
      }
    }
    if (make_is_one_of(make_special_targets, line + i, word_end - i)) {
      make_mark(ctx, i, word_end, EHighlightToken_Keyword);
    } else {
      make_mark(ctx, i, word_end, EHighlightToken_Keyword2);
      for (int k = i; k < word_end;) {
        if (line[k] == '$') {
          k = make_ref(ctx, k, word_end);
        } else {
          if (line[k] == '%') {
            ctx->tokens[k] = (char)EHighlightToken_Symbol2;
          }
          ++k;
        }
      }
    }
    i = word_end;
  }
}

static bool make_statement(HighlightContext* ctx, int i);

static void make_assignment(HighlightContext* ctx, int i, int op_start, int op_end) {
  int name_end = op_start;
  while (name_end > i && make_is_space(ctx->line[name_end - 1])) {
    --name_end;
  }
  make_mark(ctx, i, name_end, EHighlightToken_Type);
  for (int k = i; k < name_end;) {
    k = ctx->line[k] == '$' ? make_ref(ctx, k, name_end) : k + 1;
  }
  make_mark(ctx, op_start, op_end, EHighlightToken_Symbol);
  make_value(ctx, op_end, ctx->len, false);
}

static void make_rule(HighlightContext* ctx, int i, int op_start, int op_end) {
  const char* line = ctx->line;
  make_targets(ctx, i, op_start);
  make_mark(ctx, op_start, op_end, EHighlightToken_Symbol);
  // target-specific variable: "prog: CFLAGS += -g"
  int inner_start;
  int inner_end;
  if (make_classify(line, op_end, ctx->len, &inner_start, &inner_end) ==
      MakeStatement_Assignment) {
    (void)make_statement(ctx, op_end);
    return;
  }
  int k = op_end;
  while (k < ctx->len) {
    const char c = line[k];
    if (c == '$') {
      k = make_ref(ctx, k, ctx->len);
    } else if (c == '#') {
      make_mark(ctx, k, ctx->len, EHighlightToken_Comment);
      return;
    } else if (c == ';') {  // "target: prereq ; recipe"
      ctx->tokens[k] = (char)EHighlightToken_Symbol;
      make_recipe(ctx, k + 1, false);
      return;
    } else {
      if (c == '|') {
        ctx->tokens[k] = (char)EHighlightToken_Symbol;  // order-only prerequisites
      } else if (c == '%') {
        ctx->tokens[k] = (char)EHighlightToken_Symbol2;
      }
      ++k;
    }
  }
}

// A line that is not a recipe, from its first non-blank character `i`.
// Returns whether it starts a define ... endef body.
static bool make_statement(HighlightContext* ctx, int i) {
  const char* line = ctx->line;
  for (;;) {
    i = make_skip_space(line, i, ctx->len);
    if (i >= ctx->len) {
      return false;
    }
    if (line[i] == '#') {
      make_mark(ctx, i, ctx->len, EHighlightToken_Comment);
      return false;
    }
    const int word_end = make_word_end(line, i, ctx->len);
    int op_start;
    int op_end;
    const MakeStatement statement = make_classify(line, i, ctx->len, &op_start, &op_end);
    // "export = x" sets a variable called export; "ifeq (a,b)" has no space
    // before its parenthesis in some makefiles
    int directive_end = word_end;
    for (int k = i; k < word_end; ++k) {
      if (line[k] == '(') {
        directive_end = k;
        break;
      }
    }
    const bool directive =
      make_is_one_of(make_directives, line + i, directive_end - i) &&
      !(statement != MakeStatement_Other && op_start == make_skip_space(line, word_end, ctx->len));
    if (!directive) {
      if (statement == MakeStatement_Assignment) {
        make_assignment(ctx, i, op_start, op_end);
      } else if (statement == MakeStatement_Rule) {
        make_rule(ctx, i, op_start, op_end);
      } else {
        make_value(ctx, i, ctx->len, false);
      }
      return false;
    }
    make_mark(ctx, i, directive_end, EHighlightToken_Keyword);
    if (directive_end - i == 6 && strncmp(line + i, "define", 6) == 0) {
      const int name = make_skip_space(line, directive_end, ctx->len);
      const int name_end = make_word_end(line, name, ctx->len);
      make_mark(ctx, name, name_end, EHighlightToken_Type);
      const int op = make_skip_space(line, name_end, ctx->len);
      make_mark(ctx, op, ctx->len, EHighlightToken_Symbol);
      return true;
    }
    if (!make_is_one_of(make_modifiers, line + i, directive_end - i)) {
      make_value(ctx, directive_end, ctx->len, true);
      return false;
    }
    i = directive_end;
  }
}

// Whether the line's first word is `word`.
static bool make_first_word_is(const char* line, int i, int length, const char* word) {
  const int end = make_word_end(line, i, length);
  return (int)strlen(word) == end - i && strncmp(line + i, word, (size_t)(end - i)) == 0;
}

static void make_highlight(HighlightContext* ctx) {
  if (ctx == NULL || ctx->line == NULL || ctx->tokens == NULL) {
    return;
  }
  const char* line = ctx->line;
  const int state = ctx->lang_state;
  make_mark(ctx, 0, ctx->len, EHighlightToken_Normal);
  ctx->in_multiline_comment = false;
  const bool continues = make_continues(line, ctx->len);

  if (state >= MakeState_Define) {
    const int first = make_skip_space(line, 0, ctx->len);
    int depth = state - MakeState_Define + 1;
    if (make_first_word_is(line, first, ctx->len, "endef")) {
      make_mark(ctx, first, first + 5, EHighlightToken_Keyword);
      --depth;
    } else {
      if (make_first_word_is(line, first, ctx->len, "define")) {
        make_mark(ctx, first, first + 6, EHighlightToken_Keyword);
        ++depth;
      }
      make_value(ctx, first, ctx->len, false);
    }
    ctx->lang_state = depth > 0 ? MakeState_Define + depth - 1 : MakeState_Normal;
    ctx->in_string = false;
    ctx->string_char = 0;
    return;
  }

  if (state == MakeState_Comment) {
    make_mark(ctx, 0, ctx->len, EHighlightToken_Comment);
    ctx->lang_state = continues ? MakeState_Comment : MakeState_Normal;
    return;
  }

  if (state == MakeState_Value) {  // even when it starts with a tab
    ctx->in_string = false;
    ctx->string_char = 0;
    make_value(ctx, 0, ctx->len, false);
    ctx->lang_state = continues ? MakeState_Value : MakeState_Normal;
    return;
  }

  if (state == MakeState_Recipe || (ctx->len > 0 && line[0] == '\t')) {
    make_recipe(ctx, 0, state == MakeState_Recipe);
    ctx->lang_state = continues ? MakeState_Recipe : MakeState_Normal;
    if (!continues) {
      ctx->in_string = false;
      ctx->string_char = 0;
    }
    return;
  }
  ctx->in_string = false;
  ctx->string_char = 0;

  const int first = make_skip_space(line, 0, ctx->len);
  if (first < ctx->len && line[first] == '#') {
    make_mark(ctx, first, ctx->len, EHighlightToken_Comment);
    ctx->lang_state = continues ? MakeState_Comment : MakeState_Normal;
    return;
  }
  if (make_statement(ctx, first)) {
    ctx->lang_state = MakeState_Define;
  } else {
    ctx->lang_state = continues ? MakeState_Value : MakeState_Normal;
  }
}

bool makefile_is_rule(const char* text, int length) {
  if (length == 0 || text[0] == '\t') {
    return false;
  }
  const int first = make_skip_space(text, 0, length);
  if (first >= length || text[first] == '#') {
    return false;
  }
  int directive_end = make_word_end(text, first, length);
  for (int k = first; k < directive_end; ++k) {
    if (text[k] == '(') {
      directive_end = k;
      break;
    }
  }
  if (make_is_one_of(make_directives, text + first, directive_end - first)) {
    return false;
  }
  int op_start;
  int op_end;
  if (make_classify(text, first, length, &op_start, &op_end) != MakeStatement_Rule) {
    return false;
  }
  // "target: VAR = value" sets a variable, no recipe follows it
  return make_classify(text, op_end, length, &op_start, &op_end) !=
         MakeStatement_Assignment;
}

static const Highlighter make_highlighter = {
  .name = "make",
  .highlight_line = make_highlight,
};

const Filetype g_filetype_make = {
  .name = "make",
  .extensions = make_extensions,
  .filenames = make_filenames,
  .highlighter = &make_highlighter,
  .completion_words = completion_words,
  .word_prefix_chars = ".",
  .hard_tabs = true,
};
