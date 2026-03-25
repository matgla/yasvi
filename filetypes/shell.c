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

#include "shell.h"

#include <stdbool.h>
#include <string.h>

static const char* shell_extensions[] = {".sh", ".bash", ".zsh", ".fish", ".ksh", ".dash", NULL};
static const char* shell_shebangs[] = {"sh", "bash", "zsh", "fish", "ksh", "dash", "/env sh", "/env bash", NULL};

// Shell keywords
static const char* keywords[] = {
  "if",      "then",    "else",    "elif",   "fi",
  "for",     "do",      "done",    "while",  "until",
  "case",    "in",      "esac",    "select", "function",
  "return",  "break",   "continue","shift",  "exit",
  "source",  ".",       NULL};

// Shell builtins
static const char* builtins[] = {
  "echo",    "printf",  "cd",      "pwd",     "pushd",   "popd",
  "dirs",    "export",  "unset",   "env",     "set",
  "read",    "readarray", "mapfile",
  "declare", "typeset", "local",   "readonly",
  "alias",   "unalias", "eval",    "exec",
  "test",    "[",       "[[",
  "trap",    "wait",    "jobs",    "fg",      "bg",
  "kill",    "ulimit",  "umask",   "times",
  "true",    "false",   NULL};

// Secondary keywords/constants
static const char* keywords_2[] = {
  "true", "false", NULL};

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

static bool is_word_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

static void shell_highlight(HighlightContext* ctx) {
  if (!ctx || !ctx->line || !ctx->tokens) {
    return;
  }

  bool in_string = ctx->in_string;
  char string_char = ctx->string_char;

  int token_start = -1;

  for (int i = 0; i < ctx->len; ++i) {
    // Handle comments (only outside strings)
    if (!in_string && ctx->line[i] == '#') {
      // Comment - mark rest of line
      for (int j = i; j < ctx->len; ++j) {
        ctx->tokens[j] = (char)EHighlightToken_Comment;
      }
      break;  // Done with this line
    }

    if (in_string) {
      ctx->tokens[i] = (char)EHighlightToken_String;

      if (string_char == '"') {
        // Double-quoted string: variables and escapes still work
        if (ctx->line[i] == '\\') {
          ctx->tokens[i] = (char)EHighlightToken_Digit;
          if (i + 1 < ctx->len) {
            ++i;
            ctx->tokens[i] = (char)EHighlightToken_Digit;
          }
        } else if (ctx->line[i] == '$' && i + 1 < ctx->len &&
                   (is_word_char(ctx->line[i + 1]) || ctx->line[i + 1] == '{')) {
          // Variable in string
          ctx->tokens[i] = (char)EHighlightToken_Preprocessor;  // Use preprocessor color for vars
          ++i;
          if (ctx->line[i] == '{') {
            ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
            ++i;
            while (i < ctx->len && ctx->line[i] != '}') {
              ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
              ++i;
            }
            if (i < ctx->len) {
              ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
            }
          } else {
            while (i < ctx->len && is_word_char(ctx->line[i])) {
              ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
              ++i;
            }
            --i;  // Adjust for loop increment
          }
        } else if (ctx->line[i] == '"') {
          in_string = false;
          ctx->in_string = false;
          ctx->string_char = 0;
        }
      } else {
        // Single-quoted string: literal, only ' ends it
        if (ctx->line[i] == string_char) {
          in_string = false;
          ctx->in_string = false;
          ctx->string_char = 0;
        }
      }
    } else if (ctx->line[i] == '"' || ctx->line[i] == '\'') {
      // Start of string
      string_char = ctx->line[i];
      in_string = true;
      ctx->in_string = true;
      ctx->string_char = string_char;
      ctx->tokens[i] = (char)EHighlightToken_String;
    } else if (ctx->line[i] == '$') {
      // Variable
      ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
      if (i + 1 < ctx->len) {
        ++i;
        if (ctx->line[i] == '{') {
          // ${VAR} style
          ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
          ++i;
          while (i < ctx->len && ctx->line[i] != '}') {
            ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
            ++i;
          }
          if (i < ctx->len) {
            ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
          }
        } else if (is_word_char(ctx->line[i]) || ctx->line[i] == '*' ||
                   ctx->line[i] == '@' || ctx->line[i] == '#' ||
                   ctx->line[i] == '?' || ctx->line[i] == '-' ||
                   ctx->line[i] == '$' || ctx->line[i] == '!') {
          // $VAR or special vars
          ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
          ++i;
          while (i < ctx->len && is_word_char(ctx->line[i])) {
            ctx->tokens[i] = (char)EHighlightToken_Preprocessor;
            ++i;
          }
          --i;  // Adjust for loop increment
        }
      }
    } else if (ctx->line[i] == '\\') {
      // Escape sequence
      ctx->tokens[i] = (char)EHighlightToken_Digit;
      if (i + 1 < ctx->len) {
        ++i;
        ctx->tokens[i] = (char)EHighlightToken_Digit;
      }
    } else if (is_word_char(ctx->line[i])) {
      // Word character - part of a token
      if (token_start == -1) {
        token_start = i;
      }
      ctx->tokens[i] = (char)EHighlightToken_Normal;
    } else {
      // Non-word character - end current token
      if (token_start != -1) {
        int token_len = i - token_start;
        if (is_token(keywords, &ctx->line[token_start], token_len)) {
          for (int j = token_start; j < i; ++j) {
            ctx->tokens[j] = (char)EHighlightToken_Keyword;
          }
        } else if (is_token(builtins, &ctx->line[token_start], token_len)) {
          for (int j = token_start; j < i; ++j) {
            ctx->tokens[j] = (char)EHighlightToken_Type;  // Use Type color for builtins
          }
        } else if (is_token(keywords_2, &ctx->line[token_start], token_len)) {
          for (int j = token_start; j < i; ++j) {
            ctx->tokens[j] = (char)EHighlightToken_Keyword2;
          }
        }
        token_start = -1;
      }

      // Operators and symbols
      if (ctx->line[i] == '|' || ctx->line[i] == '&' || ctx->line[i] == ';' ||
          ctx->line[i] == '>' || ctx->line[i] == '<' || ctx->line[i] == '=' ||
          ctx->line[i] == '(' || ctx->line[i] == ')' || ctx->line[i] == '{' ||
          ctx->line[i] == '}' || ctx->line[i] == '[' || ctx->line[i] == ']' ||
          ctx->line[i] == '+' || ctx->line[i] == '-' || ctx->line[i] == '*' ||
          ctx->line[i] == '?' || ctx->line[i] == '!') {
        ctx->tokens[i] = (char)EHighlightToken_Symbol;
      } else if (ctx->line[i] >= '0' && ctx->line[i] <= '9') {
        ctx->tokens[i] = (char)EHighlightToken_Digit;
      } else {
        ctx->tokens[i] = (char)EHighlightToken_Normal;
      }
    }
  }

  // Handle token at end of line
  if (token_start != -1) {
    int token_len = ctx->len - token_start;
    if (is_token(keywords, &ctx->line[token_start], token_len)) {
      for (int j = token_start; j < ctx->len; ++j) {
        ctx->tokens[j] = (char)EHighlightToken_Keyword;
      }
    } else if (is_token(builtins, &ctx->line[token_start], token_len)) {
      for (int j = token_start; j < ctx->len; ++j) {
        ctx->tokens[j] = (char)EHighlightToken_Type;
      }
    } else if (is_token(keywords_2, &ctx->line[token_start], token_len)) {
      for (int j = token_start; j < ctx->len; ++j) {
        ctx->tokens[j] = (char)EHighlightToken_Keyword2;
      }
    }
  }

  // Propagate string state back
  ctx->in_string = in_string;
  ctx->string_char = string_char;
}

static const Highlighter shell_highlighter = {
  .name = "shell",
  .highlight_line = shell_highlight,
};

const Filetype g_filetype_shell = {
  .name = "shell",
  .extensions = shell_extensions,
  .shebangs = shell_shebangs,
  .highlighter = &shell_highlighter,
};
