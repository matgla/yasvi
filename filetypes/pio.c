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

// RP2040/RP2350 PIO assembly, as apps/pioasm reads it. The `% lang {` ... `%}`
// code blocks are highlighted in their own language: c-sdk as C, zig as Zig.

#include "pio.h"

#include <stdbool.h>
#include <string.h>

#include "c.h"
#include "zig.h"

static const char* pio_extensions[] = {".pio", NULL};

// Directives, without the leading '.'
static const char* directives[] = {
  "program",   "wrap_target", "wrap",     "word",       "define",
  "side_set",  "origin",      "lang_opt", "pio_version", "clock_div",
  "fifo",      "mov_status",  "set",      "out",        "in",
  NULL};

// Instructions
static const char* keywords_1[] = {
  "jmp", "wait", "in", "out", "push", "pull", "mov", "irq", "set", "nop",
  NULL};

// Sources and destinations
static const char* types[] = {
  "pins",   "pindirs", "x",      "y",      "pc",     "exec",
  "isr",    "osr",     "null",   "status", "gpio",   "pin",
  "jmppin", "osre",    "rxfifo", "txfifo", NULL};

// Modifiers, options and values
static const char* keywords_2[] = {
  "side",    "sideset", "side_set", "opt",     "optional", "public",
  "block",   "noblock", "iffull",   "ifempty", "rel",      "clear",
  "nowait",  "next",    "prev",     "txrx",    "tx",       "rx",
  "txput",   "txget",   "putget",   "rp2040",  "rp2350",   "left",
  "right",   "auto",    "manual",   "one",     "zero",     NULL};

static const char* symbols = "+-|<>=:!(),~^/";
static const char* symbols2 = "*&[]";

// lang_state: which language the line starts in
enum {
  PioLang_Pio = 0,
  PioLang_C,
  PioLang_Zig,
  PioLang_Other,  // a code block for an output with no highlighter (python...)
};

static char lower(char c) {
  return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

// pioasm matches keywords and directives ignoring case
static bool word_is(const char* word, int n, const char* keyword) {
  int i = 0;
  while (i < n && keyword[i] != '\0' && lower(word[i]) == keyword[i]) {
    ++i;
  }
  return i == n && keyword[i] == '\0';
}

static bool is_token(const char** array, const char* word, int n) {
  for (const char** kw = array; *kw != NULL; ++kw) {
    if (word_is(word, n, *kw)) {
      return true;
    }
  }
  return false;
}

static bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

static bool is_word_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_word_char(char c) {
  return is_word_start(c) || is_digit(c);
}

static bool is_blank(char c) {
  return c == ' ' || c == '\t' || c == '\r';
}

static void mark_range(HighlightContext* ctx, int start, int end,
                       EHighlightToken token) {
  for (int i = start; i < end; ++i) {
    ctx->tokens[i] = (char)token;
  }
}

static int scan_word(const char* line, int len, int i) {
  while (i < len && is_word_char(line[i])) {
    ++i;
  }
  return i;
}

// 42, 1.5, .5, 0x1f, 0b101
static int scan_number(const char* line, int len, int i) {
  while (i < len && (is_word_char(line[i]) ||
                     (line[i] == '.' && i + 1 < len && is_digit(line[i + 1])))) {
    ++i;
  }
  return i;
}

// Run the code block's language over line[start..len)
static void highlight_embedded(HighlightContext* ctx, int start) {
  const Highlighter* highlighter = NULL;
  if (ctx->lang_state == PioLang_C) {
    highlighter = g_filetype_c.highlighter;
  } else if (ctx->lang_state == PioLang_Zig) {
    highlighter = g_filetype_zig.highlighter;
  }

  if (highlighter == NULL) {
    mark_range(ctx, start, ctx->len, EHighlightToken_Normal);
    return;
  }

  HighlightContext sub = *ctx;
  sub.line += start;
  sub.len -= start;
  sub.tokens += start;
  sub.lang_state = 0;
  highlighter->highlight_line(&sub);
  ctx->in_multiline_comment = sub.in_multiline_comment;
  ctx->in_string = sub.in_string;
  ctx->string_char = sub.string_char;
}

// `% lang {` at i: like pioasm, the block opens at the last '{' before the end
// of the line or the next '%', with something between it and the '%'.
// Returns the index of that '{', or -1 when this is not a block start.
static int find_code_block_open(const char* line, int len, int i) {
  int open = -1;
  for (int p = i + 1; p < len && line[p] != '%'; ++p) {
    if (line[p] == '{' && p > i + 1) {
      open = p;
    }
  }
  return open;
}

static int code_block_language(const char* line, int start, int end) {
  while (start < end && is_blank(line[start])) {
    ++start;
  }
  while (end > start && is_blank(line[end - 1])) {
    --end;
  }
  int n = end - start;
  if (n == 5 && strncmp(&line[start], "c-sdk", 5) == 0) {
    return PioLang_C;
  }
  if (n == 3 && strncmp(&line[start], "zig", 3) == 0) {
    return PioLang_Zig;
  }
  return PioLang_Other;
}

// "%}" at the start of a line, followed only by blanks, closes a code block
static bool is_code_block_end(const char* line, int len) {
  if (len < 2 || line[0] != '%' || line[1] != '}') {
    return false;
  }
  for (int i = 2; i < len; ++i) {
    if (!is_blank(line[i])) {
      return false;
    }
  }
  return true;
}

static void highlight_word(HighlightContext* ctx, int start, int end) {
  const char* word = &ctx->line[start];
  int n = end - start;
  EHighlightToken token = EHighlightToken_Normal;
  if (is_token(keywords_1, word, n)) {
    token = EHighlightToken_Keyword;
  } else if (is_token(types, word, n)) {
    token = EHighlightToken_Type;
  } else if (is_token(keywords_2, word, n)) {
    token = EHighlightToken_Keyword2;
  }
  mark_range(ctx, start, end, token);
}

static void pio_highlight(HighlightContext* ctx) {
  if (!ctx || !ctx->line || !ctx->tokens) {
    return;
  }

  const char* line = ctx->line;
  int len = ctx->len;

  if (ctx->lang_state != PioLang_Pio) {
    if (is_code_block_end(line, len)) {
      mark_range(ctx, 0, 2, EHighlightToken_Preprocessor);
      mark_range(ctx, 2, len, EHighlightToken_Normal);
      ctx->lang_state = PioLang_Pio;
      ctx->in_multiline_comment = false;
      ctx->in_string = false;
      ctx->string_char = 0;
    } else {
      highlight_embedded(ctx, 0);
    }
    return;
  }

  // A C string never spans lines in pio; only a /* comment carries over
  ctx->in_string = false;
  ctx->string_char = 0;

  // Rest of a .lang_opt line: a free-form name = value for the output
  bool lang_opt = false;
  int i = 0;

  while (i < len) {
    char c = line[i];
    if (ctx->in_multiline_comment) {
      if (c == '*' && i + 1 < len && line[i + 1] == '/') {
        mark_range(ctx, i, i + 2, EHighlightToken_Comment);
        ctx->in_multiline_comment = false;
        i += 2;
      } else {
        ctx->tokens[i++] = (char)EHighlightToken_Comment;
      }
    } else if (!lang_opt &&
               (c == ';' || (c == '/' && i + 1 < len && line[i + 1] == '/'))) {
      mark_range(ctx, i, len, EHighlightToken_Comment);
      break;
    } else if (!lang_opt && c == '/' && i + 1 < len && line[i + 1] == '*') {
      mark_range(ctx, i, i + 2, EHighlightToken_Comment);
      ctx->in_multiline_comment = true;
      i += 2;
    } else if (c == '%' && !lang_opt) {
      int open = find_code_block_open(line, len, i);
      if (open < 0) {
        ctx->tokens[i++] = (char)EHighlightToken_Normal;
        continue;
      }
      mark_range(ctx, i, open + 1, EHighlightToken_Preprocessor);
      ctx->lang_state = code_block_language(line, i + 1, open);
      highlight_embedded(ctx, open + 1);
      return;
    } else if (c == '"') {
      // Only .lang_opt has strings; like pioasm, one runs to the last quote
      int close = -1;
      for (int p = i + 1; p < len; ++p) {
        if (line[p] == '"') {
          close = p;
        }
      }
      int end = close < 0 ? len : close + 1;
      mark_range(ctx, i, end, EHighlightToken_String);
      i = end;
    } else if (is_digit(c) || (c == '.' && i + 1 < len && is_digit(line[i + 1]))) {
      int end = scan_number(line, len, i);
      mark_range(ctx, i, end, EHighlightToken_Digit);
      i = end;
    } else if (c == '.' && !lang_opt && i + 1 < len && is_word_start(line[i + 1])) {
      int end = scan_word(line, len, i + 1);
      bool known = is_token(directives, &line[i + 1], end - i - 1);
      mark_range(ctx, i, end,
                 known ? EHighlightToken_Preprocessor : EHighlightToken_Normal);
      lang_opt = word_is(&line[i + 1], end - i - 1, "lang_opt");
      i = end;
    } else if (is_word_start(c)) {
      int end = scan_word(line, len, i);
      if (lang_opt) {
        mark_range(ctx, i, end, EHighlightToken_Normal);
      } else {
        highlight_word(ctx, i, end);
      }
      i = end;
    } else {
      if (c != '\0' && strchr(symbols, c) != NULL) {
        ctx->tokens[i] = (char)EHighlightToken_Symbol;
      } else if (c != '\0' && strchr(symbols2, c) != NULL) {
        ctx->tokens[i] = (char)EHighlightToken_Symbol2;
      } else {
        ctx->tokens[i] = (char)EHighlightToken_Normal;
      }
      ++i;
    }
  }
}

static const Highlighter pio_highlighter = {
  .name = "pio",
  .highlight_line = pio_highlight,
};

static const char* completion_directives[] = {
  ".program",  ".wrap_target", ".wrap",     ".word",        ".define",
  ".side_set", ".origin",      ".lang_opt", ".pio_version", ".clock_div",
  ".fifo",     ".mov_status",  NULL};

static const char** completion_words[] = {
  keywords_1, types, keywords_2, completion_directives, NULL};

const Filetype g_filetype_pio = {
  .name = "pio",
  .extensions = pio_extensions,
  .shebangs = NULL,
  .highlighter = &pio_highlighter,
  .completion_words = completion_words,
  .word_prefix_chars = ".",
};
