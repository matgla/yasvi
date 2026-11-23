/*
 Copyright (c) 2025 Mateusz Stadnik

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

#include "c.h"

#include <stdbool.h>
#include <string.h>

static const char* c_extensions[] = {".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", NULL};

// C keywords
static const char* keywords_1[] = {
  "if",       "else",     "while",    "for",      "return",   "break",
  "continue", "switch",   "case",     "default",  "do",       "goto",
  "typedef",  "struct",   "union",    "static",   "extern",   "inline",
  "const",    "volatile", "enum",     "register", "restrict", "sizeof",
  "auto",     "_Alignas", "_Alignof", "_Atomic",  "_Generic", "_Noreturn",
  "_Static_assert",       "_Thread_local",        "alignas",  "alignof",
  "constexpr",            "static_assert",        "thread_local",
  "typeof",   "typeof_unqual",        "asm",      "__asm__",  "__attribute__",
  NULL};

// C types
static const char* types[] = {
  "int",      "char",   "float",   "double",  "void",
  "bool",     "short",  "long",    "size_t",  "ssize_t",
  "int8_t",   "int16_t","int32_t", "int64_t",
  "uint8_t",  "uint16_t","uint32_t","uint64_t",
  "uintptr_t","intptr_t","off_t",  "FILE",    "ptrdiff_t",
  "unsigned", "signed", "_Bool",   "wchar_t", "va_list", NULL};

// The yasc dialect of tinycc (its default; -ccompat is regular C). tcc takes
// these words as keywords only where its parser expects them, and as names
// everywhere else, so each one is checked against its neighbours
// (yasc_word_token). The same rules as tools/vscode-yasc.
static const char* yasc_keywords[] = {
  "owned",    "view",       "maybe",      "out",    "drop",   "defer",
  "errdefer", "try",        "catch",      "error",  "module", "import",
  "export",   "error_name", "error_from", "move",   "borrow", NULL};

// lang_state bits
#define C_STATE_ERROR_SET 0x01   // inside the braces of `error Set { ... }`
#define C_STATE_ERROR_NAME 0x02  // `error Set` ended the line: a '{' may follow

// Secondary keywords/constants
static const char* keywords_2[] = {
  "false", "true", "NULL", "FALSE", "TRUE", "nullptr", NULL};

// Symbols
static const char* symbols = "+-|<>=:?!(),;/";
static const char* symbols2 = "*&{}[]";
static const char* whitespace_symbols = " \f\n\r\t\v";
static const char* include_symbols = "\"<>";
static const char* string_symbols = "\"'";

static bool is_token(const char** array, const char* word, int n) {
  for (const char** kw = array; *kw != NULL; ++kw) {
    if ((int)strlen(*kw) != n) {
      continue;
    }
    if (strncmp(*kw, word, n) == 0) {
      return true;
    }
  }
  return false;
}

static bool is_ident_start(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool is_ident_char(char c) {
  return is_ident_start(c) || (c >= '0' && c <= '9');
}

static bool is_space(char c) {
  return c == ' ' || c == '\t';
}

static bool word_is(const char* line, int start, int end, const char* word) {
  const int n = (int)strlen(word);
  return end - start == n && strncmp(&line[start], word, n) == 0;
}

// The first non-blank position at or after `i` (ctx->len at the end)
static int skip_blanks(const HighlightContext* ctx, int i) {
  while (i < ctx->len && is_space(ctx->line[i])) {
    ++i;
  }
  return i;
}

// The last non-blank position before `i` (-1 at the start of the line)
static int skip_blanks_back(const HighlightContext* ctx, int i) {
  --i;
  while (i >= 0 && is_space(ctx->line[i])) {
    --i;
  }
  return i;
}

// The end of the word that starts at `i`
static int word_end(const HighlightContext* ctx, int i) {
  while (i < ctx->len && is_ident_char(ctx->line[i])) {
    ++i;
  }
  return i;
}

// The word that ends just before `end` (blanks skipped): its start, or -1
static int word_before(const HighlightContext* ctx, int end, int* word_stop) {
  const int last = skip_blanks_back(ctx, end);
  if (last < 0 || !is_ident_char(ctx->line[last])) {
    return -1;
  }
  int start = last;
  while (start > 0 && is_ident_char(ctx->line[start - 1])) {
    --start;
  }
  *word_stop = last + 1;
  return start;
}

// `s.word`, `p->word`: a member, never a keyword
static bool after_member_access(const HighlightContext* ctx, int start) {
  const int p = skip_blanks_back(ctx, start);
  return p >= 0 && (ctx->line[p] == '.' || (ctx->line[p] == '>' && p > 0 && ctx->line[p - 1] == '-'));
}

static bool is_qualifier_after_star(const char* line, int start, int end) {
  return word_is(line, start, end, "owned") || word_is(line, start, end, "view") ||
         word_is(line, start, end, "maybe");
}

// `error` (not a member) just before `start`, as in `error Set` or `error.Name`
static bool error_keyword_before(const HighlightContext* ctx, int start, char sep) {
  int p = sep == '.' ? start - 1 : start;
  if (sep == '.') {
    if (p < 0 || ctx->line[p] != '.') {
      return false;
    }
  }
  int stop = 0;
  const int w = word_before(ctx, p, &stop);
  if (w < 0 || (sep == '.' && stop != p) || !word_is(ctx->line, w, stop, "error")) {
    return false;
  }
  return !after_member_access(ctx, w);
}

static bool only_blanks_before(const HighlightContext* ctx, int start) {
  return skip_blanks_back(ctx, start) < 0;
}

// `module a.b;` / `import a.b;`: the rest of the line after the keyword
static bool module_name_follows(const HighlightContext* ctx, int end) {
  int i = skip_blanks(ctx, end);
  if (i == end || i == ctx->len || !is_ident_start(ctx->line[i])) {
    return false;
  }
  while (i < ctx->len && (is_ident_char(ctx->line[i]) || ctx->line[i] == '.')) {
    ++i;
  }
  i = skip_blanks(ctx, i);
  return i < ctx->len && ctx->line[i] == ';';
}

// The token of a yasc word in this position, or -1 when it is a name here
static int yasc_word_token(const HighlightContext* ctx, int start, int end) {
  const char* line = ctx->line;
  if (after_member_access(ctx, start)) {
    return -1;
  }
  const int next_at = skip_blanks(ctx, end);
  const char next = next_at < ctx->len ? line[next_at] : '\0';
  const bool eol = next_at == ctx->len;

  // owned / view / maybe qualify a pointer: after its '*' (or another of them)
  if (is_qualifier_after_star(line, start, end)) {
    const int p = skip_blanks_back(ctx, start);
    int stop = 0;
    const int w = word_before(ctx, start, &stop);
    const bool after_star = (p >= 0 && line[p] == '*') ||
                            (w >= 0 && is_qualifier_after_star(line, w, stop));
    return after_star && (eol || is_ident_start(next) || next == '*' || next == ')' || next == ',')
               ? EHighlightToken_Keyword
               : -1;
  }
  // out / drop come before a parameter's type
  if (word_is(line, start, end, "out") || word_is(line, start, end, "drop")) {
    return next_at > end && is_ident_start(next) ? EHighlightToken_Keyword : -1;
  }
  if (word_is(line, start, end, "defer") || word_is(line, start, end, "errdefer")) {
    return eol || is_ident_start(next) || next == '*' || next == '{' || next == '('
               ? EHighlightToken_Keyword
               : -1;
  }
  if (word_is(line, start, end, "try")) {
    return is_ident_start(next) || next == '(' || next == '*' || next == '&' ? EHighlightToken_Keyword
                                                                             : -1;
  }
  // f() catch X, catch |e| {...}: after the end of an expression, or first on a
  // continuation line
  if (word_is(line, start, end, "catch")) {
    if (next == '|' || next == '{') {
      return EHighlightToken_Keyword;
    }
    if (eol || strchr(";,=)].:", next) != NULL) {
      return -1;
    }
    const int p = skip_blanks_back(ctx, start);
    if (p < 0) {
      return next != '(' ? EHighlightToken_Keyword : -1;
    }
    return is_ident_char(line[p]) || strchr(")]\"'", line[p]) != NULL ? EHighlightToken_Keyword : -1;
  }
  // error.Name, error Set { ... }, error Set e, error any
  if (word_is(line, start, end, "error")) {
    if (next_at == end && next == '.' && end + 1 < ctx->len && line[end + 1] >= 'A' &&
        line[end + 1] <= 'Z') {
      return EHighlightToken_Keyword;
    }
    return next_at > end && is_ident_start(next) ? EHighlightToken_Keyword : -1;
  }
  if (word_is(line, start, end, "error_name") || word_is(line, start, end, "error_from") ||
      word_is(line, start, end, "move") || word_is(line, start, end, "borrow")) {
    return next == '(' ? EHighlightToken_Keyword : -1;
  }
  // module a.b; import a.b; export DECL -- at the start of a declaration
  if (word_is(line, start, end, "module") || word_is(line, start, end, "import")) {
    return only_blanks_before(ctx, start) && module_name_follows(ctx, end) ? EHighlightToken_Keyword
                                                                          : -1;
  }
  if (word_is(line, start, end, "export")) {
    return only_blanks_before(ctx, start) && next_at > end && is_ident_start(next)
               ? EHighlightToken_Keyword
               : -1;
  }
  return -1;
}

// A word after a yasc `error`: the name in error.Name, the set's name in
// `error Set`, or a member of the set being declared
static int yasc_error_word_token(HighlightContext* ctx, int start, int end) {
  if (ctx->lang_state & C_STATE_ERROR_SET) {
    return EHighlightToken_Keyword2;
  }
  if (start > 0 && ctx->line[start - 1] == '.' && ctx->line[start] >= 'A' && ctx->line[start] <= 'Z' &&
      error_keyword_before(ctx, start, '.')) {
    return EHighlightToken_Keyword2;
  }
  if (error_keyword_before(ctx, start, ' ')) {
    const int next_at = skip_blanks(ctx, end);
    if (next_at < ctx->len && ctx->line[next_at] == '{') {
      ctx->lang_state |= C_STATE_ERROR_SET;  // the set's members follow
      return EHighlightToken_Type;
    }
    if (next_at == ctx->len) {
      ctx->lang_state |= C_STATE_ERROR_NAME;  // the set's '{' may start the next line
      return EHighlightToken_Type;
    }
    if (is_ident_start(ctx->line[next_at]) || ctx->line[next_at] == '*') {
      return EHighlightToken_Type;
    }
  }
  return -1;
}

// `!T`: a `!` before a type is an error union (no expression puts one there):
// `!int`, `!struct s * owned`, or `!name name(` -- a function returning
// `!typedef_t`
static bool is_error_union_bang(const HighlightContext* ctx, int i) {
  static const char* type_words[] = {"struct", "union", "enum", "const", "volatile", NULL};
  int w = skip_blanks(ctx, i + 1);
  if (w == ctx->len || !is_ident_start(ctx->line[w])) {
    return false;
  }
  int e = word_end(ctx, w);
  if (is_token(types, &ctx->line[w], e - w) || is_token(type_words, &ctx->line[w], e - w)) {
    return true;
  }
  if (word_is(ctx->line, w, e, "sizeof") || word_is(ctx->line, w, e, "defined") ||
      word_is(ctx->line, w, e, "return")) {
    return false;
  }
  // name, then '*' and pointer qualifiers, then blanks, a name and '('
  int j = e;
  for (;;) {
    const int k = skip_blanks(ctx, j);
    if (k < ctx->len && ctx->line[k] == '*') {
      j = k + 1;
      continue;
    }
    if (k < ctx->len && is_ident_start(ctx->line[k])) {
      const int ke = word_end(ctx, k);
      if (is_qualifier_after_star(ctx->line, k, ke)) {
        j = ke;
        continue;
      }
    }
    break;
  }
  const int name = skip_blanks(ctx, j);
  if (name == j && (j == e || ctx->line[j - 1] != '*')) {
    return false;  // no blank between the type and the name
  }
  if (name == ctx->len || !is_ident_start(ctx->line[name])) {
    return false;
  }
  const int paren = skip_blanks(ctx, word_end(ctx, name));
  return paren < ctx->len && ctx->line[paren] == '(';
}

static void mark(HighlightContext* ctx, int start, int end, int token) {
  for (int j = start; j < end; ++j) {
    ctx->tokens[j] = (char)token;
  }
}

static bool highlight_token(HighlightContext* ctx, int token_start, int i) {
  const int n = i - token_start;
  int token = -1;
  if (is_token(yasc_keywords, &ctx->line[token_start], n)) {
    token = yasc_word_token(ctx, token_start, i);
  }
  if (token < 0) {
    token = yasc_error_word_token(ctx, token_start, i);
  }
  if (token < 0 && is_token(keywords_1, &ctx->line[token_start], n)) {
    token = EHighlightToken_Keyword;
  }
  if (token < 0 && is_token(keywords_2, &ctx->line[token_start], n)) {
    token = EHighlightToken_Keyword2;
  }
  if (token < 0 && is_token(types, &ctx->line[token_start], n)) {
    token = EHighlightToken_Type;
  }
  if (token < 0) {
    return false;
  }
  mark(ctx, token_start, i, token);
  return true;
}

// A number from its first digit: 0x1F, 10u, 1.5e-3f, 0b101 (one pp-number)
static int number_end(const HighlightContext* ctx, int i) {
  ++i;
  while (i < ctx->len) {
    const char c = ctx->line[i];
    const char prev = ctx->line[i - 1];
    if (is_ident_char(c) || c == '.' || (c == '\'' && i + 1 < ctx->len && is_ident_char(ctx->line[i + 1]))) {
      ++i;
    } else if ((c == '+' || c == '-') && (prev == 'e' || prev == 'E' || prev == 'p' || prev == 'P')) {
      ++i;
    } else {
      break;
    }
  }
  return i;
}

static void c_highlight(HighlightContext* ctx) {
  if (!ctx || !ctx->line || !ctx->tokens) {
    return;
  }

  int preprocessor_started = 0;
  int include_started = 0;
  int escape_sequence_started = 0;

  bool in_string = ctx->in_string;
  bool in_comment = ctx->in_multiline_comment;
  char string_char = ctx->string_char;

  int token_start = -1;

  // `error Set` ended the line above: a '{' opening this one opens the set
  if (ctx->lang_state & C_STATE_ERROR_NAME) {
    ctx->lang_state &= ~C_STATE_ERROR_NAME;
    const int first = skip_blanks(ctx, 0);
    if (!in_comment && !in_string && first < ctx->len && ctx->line[first] == '{') {
      ctx->lang_state |= C_STATE_ERROR_SET;
    }
  }

  for (int i = 0; i < ctx->len; ++i) {
    if (in_comment) {
      if (ctx->line[i] == '/' && i > 0 && ctx->line[i - 1] == '*') {
        in_comment = false;
        ctx->in_multiline_comment = false;
      }
      ctx->tokens[i] = (char)EHighlightToken_Comment;
    } else if (in_string) {
      ctx->tokens[i] = (char)EHighlightToken_String;
      if (ctx->line[i] == '\\') {
        ctx->tokens[i] = (char)EHighlightToken_Digit;
        escape_sequence_started = 1;
      } else if (escape_sequence_started) {
        escape_sequence_started = 0;
        ctx->tokens[i] = (char)EHighlightToken_Digit;
      } else if (ctx->line[i] == string_char) {
        in_string = false;
        ctx->in_string = false;
        ctx->string_char = 0;
      }
    } else if (include_started) {
      if (strspn(&ctx->line[i], include_symbols) != 0) {
        ++include_started;
      }
      if (include_started == 3) {
        include_started = 0;
      }
      ctx->tokens[i] = (char)EHighlightToken_String;
    } else if (preprocessor_started) {
      if (strchr(whitespace_symbols, ctx->line[i]) != NULL) {
        ctx->tokens[i] = (char)EHighlightToken_Normal;
        if (strstr(&ctx->line[preprocessor_started], "include") ==
            &ctx->line[preprocessor_started]) {
          include_started = 1;
        }
        preprocessor_started = 0;
      } else {
        ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
      }
    } else if (strchr(string_symbols, ctx->line[i]) != NULL) {
      string_char = ctx->line[i];
      in_string = true;
      ctx->in_string = true;
      ctx->string_char = string_char;
      ctx->tokens[i] = (char)EHighlightToken_String;
    } else if (ctx->line[i] == '#') {
      preprocessor_started = i + 1;
      ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
    } else if (ctx->line[i] == '/') {
      if (i > 0) {
        if (ctx->line[i - 1] == '/') {
          // Single-line comment - mark rest of line
          for (int j = i - 1; j < ctx->len; ++j) {
            ctx->tokens[j] = (char)EHighlightToken_Comment;
          }
          break;  // Done with this line
        } else if (ctx->line[i - 1] == '*') {
          ctx->tokens[i] = (char)EHighlightToken_Comment;
          ctx->tokens[i - 1] = (char)EHighlightToken_Comment;
          ctx->in_multiline_comment = false;
          in_comment = false;
        } else {
          ctx->tokens[i] = (char)EHighlightToken_Symbol;
        }
      } else {
        ctx->tokens[i] = (char)EHighlightToken_Symbol;
      }
    } else if (ctx->line[i] == '*') {
      if (i > 0 && ctx->line[i - 1] == '/') {
        ctx->tokens[i] = (char)EHighlightToken_Comment;
        ctx->tokens[i - 1] = (char)EHighlightToken_Comment;
        in_comment = true;
        ctx->in_multiline_comment = true;
      } else {
        if (token_start != -1) {
          highlight_token(ctx, token_start, i);
          token_start = -1;
        }
        ctx->tokens[i] = (char)EHighlightToken_Symbol2;
      }
    } else if (ctx->line[i] == '\\') {
      ctx->tokens[i] = (char)EHighlightToken_Digit;
    } else {
      // Default to Normal
      ctx->tokens[i] = (char)EHighlightToken_Normal;

      if ((ctx->line[i] >= 'A' && ctx->line[i] <= 'Z') ||
          (ctx->line[i] >= 'a' && ctx->line[i] <= 'z') || (ctx->line[i] == '_') ||
          (ctx->line[i] >= '0' && ctx->line[i] <= '9')) {
        // Token character
        if (token_start == -1) {
          // Start of new token
          if (ctx->line[i] >= '0' && ctx->line[i] <= '9') {
            const int end = number_end(ctx, i);
            mark(ctx, i, end, EHighlightToken_Digit);
            i = end - 1;
            continue;
          }
          token_start = i;
        }
      } else if (token_start != -1) {
        // End of token
        highlight_token(ctx, token_start, i);
        token_start = -1;
      }

      if (token_start == -1) {
        if (ctx->line[i] == '}') {
          ctx->lang_state &= ~C_STATE_ERROR_SET;
        }
        if (ctx->line[i] == '!' && is_error_union_bang(ctx, i)) {
          ctx->tokens[i] = (char)EHighlightToken_Type;
        } else if (strchr(symbols, ctx->line[i]) != NULL) {
          ctx->tokens[i] = (char)EHighlightToken_Symbol;
        } else if (strchr(symbols2, ctx->line[i]) != NULL) {
          ctx->tokens[i] = (char)EHighlightToken_Symbol2;
        }
      }
    }
  }

  // Handle token at end of line
  if (token_start != -1) {
    highlight_token(ctx, token_start, ctx->len);
  }

  // Propagate multiline states back to context
  ctx->in_multiline_comment = in_comment;
  ctx->in_string = in_string;
  ctx->string_char = string_char;
}

static const Highlighter c_highlighter = {
  .name = "c",
  .highlight_line = c_highlight,
};

static const char** completion_words[] = {keywords_1, types, keywords_2,
                                          yasc_keywords, NULL};

const Filetype g_filetype_c = {
  .name = "c",
  .extensions = c_extensions,
  .shebangs = NULL,  // C doesn't use shebangs
  .highlighter = &c_highlighter,
  .completion_words = completion_words,
};
