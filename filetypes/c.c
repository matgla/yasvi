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
  "if",       "else",   "while",   "for",     "return",   "break",
  "continue", "switch", "case",    "default", "do",       "goto",
  "typedef",  "struct", "union",   "static",  "extern",   "inline",
  "const",    "volatile", "enum",  "register","restrict", "sizeof",
  "auto",     NULL};

// C types
static const char* types[] = {
  "int",      "char",   "float",   "double",  "void",
  "bool",     "short",  "long",    "size_t",  "ssize_t",
  "int8_t",   "int16_t","int32_t", "int64_t",
  "uint8_t",  "uint16_t","uint32_t","uint64_t",
  "uintptr_t","intptr_t","off_t",  "FILE",
  "unsigned", "signed", NULL};

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

static bool highlight_token(HighlightContext* ctx, int token_start, int i) {
  if (is_token(keywords_1, &ctx->line[token_start], i - token_start)) {
    for (int j = token_start; j < i; ++j) {
      ctx->tokens[j] = (char)EHighlightToken_Keyword;
    }
    return true;
  }

  if (is_token(keywords_2, &ctx->line[token_start], i - token_start)) {
    for (int j = token_start; j < i; ++j) {
      ctx->tokens[j] = (char)EHighlightToken_Keyword2;
    }
    return true;
  }

  if (is_token(types, &ctx->line[token_start], i - token_start)) {
    for (int j = token_start; j < i; ++j) {
      ctx->tokens[j] = (char)EHighlightToken_Type;
    }
    return true;
  }

  return false;
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
            ctx->tokens[i] = (char)EHighlightToken_Digit;
          } else {
            token_start = i;
          }
        }
      } else if (token_start != -1) {
        // End of token
        highlight_token(ctx, token_start, i);
        token_start = -1;
      }

      if (token_start == -1) {
        if (strchr(symbols, ctx->line[i]) != NULL) {
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

const Filetype g_filetype_c = {
  .name = "c",
  .extensions = c_extensions,
  .shebangs = NULL,  // C doesn't use shebangs
  .highlighter = &c_highlighter,
};
