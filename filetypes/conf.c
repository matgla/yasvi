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

#include "conf.h"

#include <stdbool.h>
#include <string.h>

static const char* conf_extensions[] = {".conf", ".cfg", ".ini", ".toml", ".yasvirc",
                                        ".gitconfig", ".gitmodules", ".editorconfig",
                                        NULL};
static const char* conf_filenames[] = {"fstab", "inittab", NULL};

// yasvi's settings (config.c, make.c): `set` lines in vi.conf and .yasvirc.
static const char* vi_settings[] = {"record",     "autoindent", "ai", "tabstop",
                                    "ts",         "expandtab",  "et", "shiftwidth",
                                    "sw",         "makeprg",    "makemode", NULL};
// Values that are words with a meaning of their own.
static const char* conf_values[] = {"on",  "off",  "yes",  "no",   "true",
                                    "false", "wait", "exec", NULL};

static const char* conf_words[] = {"set", "record", "norecord", "autoindent",
                                   "noautoindent", "tabstop", "expandtab",
                                   "noexpandtab", "shiftwidth", "makeprg", "makemode",
                                   "on", "off", "wait", "exec", NULL};
static const char** completion_words[] = {conf_words, NULL};

static bool conf_is_one_of(const char** words, const char* text, int length) {
  for (const char** word = words; *word != NULL; ++word) {
    if ((int)strlen(*word) == length && strncmp(*word, text, (size_t)length) == 0) {
      return true;
    }
  }
  return false;
}

static bool conf_is_space(char c) {
  return c == ' ' || c == '\t';
}

static bool conf_is_word_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         c == '_' || c == '-' || c == '.';
}

static void conf_mark(HighlightContext* ctx, int start, int end, EHighlightToken token) {
  for (int i = start; i < end && i < ctx->len; ++i) {
    ctx->tokens[i] = (char)token;
  }
}

// The value after the key: strings, numbers, known words, $VAR, %.
static void conf_highlight_value(HighlightContext* ctx, int i) {
  const char* line = ctx->line;
  while (i < ctx->len) {
    const char c = line[i];
    if (c == '"' || c == '\'') {
      int end = i + 1;
      while (end < ctx->len && line[end] != c) {
        end += line[end] == '\\' && c == '"' ? 2 : 1;
      }
      end = end < ctx->len ? end + 1 : ctx->len;
      conf_mark(ctx, i, end, EHighlightToken_String);
      i = end;
    } else if (c == '$' && i + 1 < ctx->len &&
               (line[i + 1] == '{' || line[i + 1] == '_' ||
                (line[i + 1] >= 'A' && line[i + 1] <= 'Z') ||
                (line[i + 1] >= 'a' && line[i + 1] <= 'z'))) {
      int end = i + 1;
      if (line[end] == '{') {
        while (end < ctx->len && line[end] != '}') {
          ++end;
        }
        end = end < ctx->len ? end + 1 : ctx->len;
      } else {
        while (end < ctx->len && conf_is_word_char(line[end]) && line[end] != '.' &&
               line[end] != '-') {
          ++end;
        }
      }
      conf_mark(ctx, i, end, EHighlightToken_Preprocessor);
      i = end;
    } else if (c == '%') {
      ctx->tokens[i++] = (char)EHighlightToken_Symbol2;  // :make's current file
    } else if (conf_is_word_char(c)) {
      int end = i;
      bool digits = true;
      while (end < ctx->len && conf_is_word_char(line[end])) {
        digits = digits && ((line[end] >= '0' && line[end] <= '9') || line[end] == '.');
        ++end;
      }
      if (digits) {
        conf_mark(ctx, i, end, EHighlightToken_Digit);
      } else if (conf_is_one_of(conf_values, line + i, end - i)) {
        conf_mark(ctx, i, end, EHighlightToken_Digit);  // a literal, as numbers
      }
      i = end;
    } else {
      ++i;
    }
  }
}

static void conf_highlight(HighlightContext* ctx) {
  if (ctx == NULL || ctx->line == NULL || ctx->tokens == NULL) {
    return;
  }
  const char* line = ctx->line;
  conf_mark(ctx, 0, ctx->len, EHighlightToken_Normal);
  ctx->in_multiline_comment = false;
  ctx->in_string = false;
  ctx->string_char = 0;

  int i = 0;
  while (i < ctx->len && conf_is_space(line[i])) {
    ++i;
  }
  if (i == ctx->len) {
    return;
  }
  if (line[i] == '#' || line[i] == ';') {
    conf_mark(ctx, i, ctx->len, EHighlightToken_Comment);
    return;
  }
  if (line[i] == '[') {  // [section]
    int end = i + 1;
    while (end < ctx->len && line[end] != ']') {
      ++end;
    }
    ctx->tokens[i] = (char)EHighlightToken_Symbol;
    conf_mark(ctx, i + 1, end, EHighlightToken_Type);
    if (end < ctx->len) {
      ctx->tokens[end] = (char)EHighlightToken_Symbol;
    }
    return;
  }
  if (ctx->len - i > 3 && strncmp(line + i, "set", 3) == 0 && conf_is_space(line[i + 3])) {
    conf_mark(ctx, i, i + 3, EHighlightToken_Preprocessor);  // vi.conf's `set`
    i += 3;
    while (i < ctx->len && conf_is_space(line[i])) {
      ++i;
    }
  }

  // The key: up to = : ? or white space.
  const int key = i;
  while (i < ctx->len && !conf_is_space(line[i]) && line[i] != '=' && line[i] != ':' &&
         line[i] != '?') {
    ++i;
  }
  const int key_length = i - key;
  if (conf_is_one_of(vi_settings, line + key, key_length)) {
    conf_mark(ctx, key, i, EHighlightToken_Keyword);
  } else if (key_length > 2 && strncmp(line + key, "no", 2) == 0 &&
             conf_is_one_of(vi_settings, line + key + 2, key_length - 2)) {
    conf_mark(ctx, key, key + 2, EHighlightToken_Digit);  // "no" turns it off, as off
    conf_mark(ctx, key + 2, i, EHighlightToken_Keyword);
  } else {
    conf_mark(ctx, key, i, EHighlightToken_Type);
  }

  while (i < ctx->len && conf_is_space(line[i])) {
    ++i;
  }
  if (i < ctx->len && (line[i] == '=' || line[i] == ':' || line[i] == '?')) {
    ctx->tokens[i++] = (char)EHighlightToken_Symbol;
  }
  conf_highlight_value(ctx, i);
}

static const Highlighter conf_highlighter = {
  .name = "conf",
  .highlight_line = conf_highlight,
};

const Filetype g_filetype_conf = {
  .name = "conf",
  .extensions = conf_extensions,
  .filenames = conf_filenames,
  .highlighter = &conf_highlighter,
  .completion_words = completion_words,
};
