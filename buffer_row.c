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

#include "buffer_row.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

const char whitespace[] = " \f\n\r\t\v";

bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

static const char* symbols = "+-|<>=:?!(),;{}/";
static const char* symbols2 = "*&{}[]";
static const char* whitespace_symbols = " \f\n\r\t\v";
static const char* include_symbols = "\"<>";
static const char* string_symbols = "\"'";
static const char* keywords_1[] = {
  "if",       "else",   "while", "for",     "return", "break",
  "continue", "switch", "case",  "default", "do",     "goto",
  "typedef",  "struct", "union", "static",  NULL};

static const char* types[] = {"int",      "char",   "float",  "double",
                              "void",     "bool",   "short",  "long",
                              "unsigned", "signed", "size_t", NULL};

static const char* keywords_2[] = {
  "false", "true", "NULL", "FALSE", "TRUE",
};

static bool is_token(const char** array, const char* word, int n) {
  for (const char** kw = array; *kw != NULL; ++kw) {
    if (strlen(*kw) != n) {
      continue;
    }
    if (strncmp(*kw, word, n) == 0) {
      return true;
    }
  }
  return false;
}

static bool highlight_token(BufferRow* row, int token_start, int i) {
  if (is_token(keywords_1, &row->data[token_start], i - token_start)) {
    for (int j = token_start; j < i; ++j) {
      row->highlight_data[j] = (char)EHighlightToken_Keyword;
    }
    return true;
  }

  if (is_token(keywords_2, &row->data[token_start], i - token_start)) {
    for (int j = token_start; j < i; ++j) {
      row->highlight_data[j] = (char)EHighlightToken_Keyword2;
    }
    return true;
  }

  if (is_token(types, &row->data[token_start], i - token_start)) {
    for (int j = token_start; j < i; ++j) {
      row->highlight_data[j] = (char)EHighlightToken_Type;
    }
    return true;
  }

  return false;
}

void buffer_row_highlight_line(BufferRow* row) {
  if (row == NULL) {
    return;  // Invalid row
  }
  int preprocessor_started = 0;
  int include_started = 0;
  int string_chars_in_row = 0;
  int string_started = 0;
  int escape_sequence_started = 0;
  bool process_next_row = true;

  while (process_next_row) {
    if (row == NULL) {
      break;
    }

    int comment_started = 0;
    row->dirty = true;
    process_next_row = false;
    if (row->prev) {
      if (row->prev->highlight_string_open && !row->highlight_comment_open) {
        string_started = row->prev->highlight_string_open;
      }
      if (row->prev->highlight_comment_open) {
        row->highlight_comment_open = row->prev->highlight_comment_open;
        comment_started = 1;
      }
    }
    int token_start = -1;

    for (int i = 0; i < row->len; ++i) {
      if (comment_started) {
        if (row->data[i] == '/' && i > 1 && row->data[i - 1] == '*') {
          row->highlight_comment_open = 0;
          comment_started = 0;
        }
        row->highlight_data[i] = (char)EHighlightToken_Comment;
      } else if (string_started) {
        row->highlight_data[i] = (char)EHighlightToken_String;
        if (row->data[i] == '\\') {
          row->highlight_data[i] = (char)EHighlightToken_Digit;
          if (i == row->len - 1) {
            escape_sequence_started = 0;
            row->highlight_string_open = string_started;
            process_next_row = true;
          } else {
            escape_sequence_started = 1;
          }
        } else if (escape_sequence_started) {
          escape_sequence_started = 0;
          row->highlight_data[i] = (char)EHighlightToken_Digit;
          if (strchr(whitespace_symbols, row->data[i]) != NULL) {
            row->highlight_string_open = string_started;
            process_next_row = true;
          }

        } else if (row->data[i] == string_started) {
          string_started = 0;
          row->highlight_string_open = 0;
        }
      } else if (include_started) {
        if (strspn(&row->data[i], include_symbols) != 0) {
          ++include_started;
        }
        if (include_started == 3) {
          include_started = 0;  // Reset after processing include
        }
        row->highlight_data[i] = (char)EHighlightToken_String;
      } else if (preprocessor_started) {
        if (strchr(whitespace_symbols, row->data[i]) != NULL) {
          row->highlight_data[i] = (char)EHighlightToken_Normal;
          if (strstr(&row->data[preprocessor_started], "include") ==
              &row->data[preprocessor_started]) {
            include_started = 1;
          }
          preprocessor_started = 0;
        } else {
          row->highlight_data[i] = (char)EHighlightToken_Preprocessor;
        }
      } else if (strchr(string_symbols, row->data[i]) != NULL) {
        string_started = row->data[i];
        row->highlight_string_open = string_started;
        row->highlight_data[i] = (char)EHighlightToken_String;
      } else if (row->data[i] == '#') {
        preprocessor_started = i + 1;
        row->highlight_data[i] = (char)EHighlightToken_Preprocessor;
      } else if (row->data[i] == '/') {
        if (i > 0) {
          if (row->data[i - 1] == '/') {
            comment_started = 1;
            row->highlight_data[i] = (char)EHighlightToken_Comment;
            row->highlight_data[i - 1] = (char)EHighlightToken_Comment;
          } else if (row->data[i - 1] == '*') {
            row->highlight_data[i] = (char)EHighlightToken_Comment;
            row->highlight_data[i - 1] = (char)EHighlightToken_Comment;
            row->highlight_comment_open = 0;
            comment_started = 0;
          }
        } else {
          row->highlight_data[i] = (char)EHighlightToken_Symbol;
        }
      } else if (row->data[i] == '*') {
        if (i > 0 && row->data[i - 1] == '/') {
          row->highlight_data[i] = (char)EHighlightToken_Comment;
          row->highlight_data[i - 1] = (char)EHighlightToken_Comment;
          row->highlight_comment_open = 1;
          comment_started = 1;
        } else {
          if (token_start != -1) {
            highlight_token(row, token_start, i);
            token_start = -1;
          }
          row->highlight_data[i] = (char)EHighlightToken_Symbol2;
        }
      } else if (row->data[i] == '\\') {
        row->highlight_data[i] = (char)EHighlightToken_Digit;
      } else {
        row->highlight_data[i] = (char)EHighlightToken_Normal;
        if ((row->data[i] >= 'A' && row->data[i] <= 'Z') ||
            (row->data[i] >= 'a' && row->data[i] <= 'z') || (row->data[i] == '_') ||
            (row->data[i] >= '0' && row->data[i] <= '9')) {
          // if token is not started
          if (token_start == -1) {
            // and if the character is a digit, we don't start a token
            if (row->data[i] >= '0' && row->data[i] <= '9') {
              row->highlight_data[i] = (char)EHighlightToken_Digit;
            } else {
              token_start = i;
            }
          }
          row->highlight_data[i] = (char)EHighlightToken_Normal;
        } else if (token_start != -1) {
          highlight_token(row, token_start, i);
          token_start = -1;
        }

        if (token_start == -1) {
          if (strchr(symbols, row->data[i]) != NULL) {
            row->highlight_data[i] = (char)EHighlightToken_Symbol;
          } else if (strchr(symbols2, row->data[i]) != NULL) {
            row->highlight_data[i] = (char)EHighlightToken_Symbol2;
          }
        }
      }
    }
    if (token_start != -1) {
      highlight_token(row, token_start, row->len);
    }

    row = row->next;
  }
}

int buffer_row_get_offset_to_first_char(const BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index >= row->len) {
    return 0;  // Invalid buffer or start index
  }

  return strspn(&row->data[start_index], whitespace);
}

bool buffer_row_has_whitespace_at_position(const BufferRow* row, int position) {
  if (row == NULL || position < 0 || position >= row->len) {
    return false;  // Invalid row or position
  }
  return strchr(whitespace, row->data[position]) != NULL;
}

int buffer_row_get_length(const BufferRow* row) {
  if (row == NULL) {
    return 0;  // Invalid row
  }
  return row->len;  // Return the length of the row
}

int buffer_row_get_offset_to_next_word(const BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index >= row->len) {
    return 0;  // Invalid row or start index
  }
  int stripped = 0;
  bool first_char = false;

  int offset = 0;
  for (offset = start_index + offset; offset < row->len; ++offset) {
    if (!first_char) {
      if (!buffer_row_has_whitespace_at_position(row, offset)) {
        first_char = true;  // Found the first non-whitespace character
      } else {
        ++stripped;
        continue;
      }
    }
    if (stripped > 0) {
      offset = stripped;
      break;
    }
    if (first_char && buffer_row_has_whitespace_at_position(row, offset)) {
      break;
    }
  }

  // strip leading whitespace
  for (; offset < row->len; ++offset) {
    if (!buffer_row_has_whitespace_at_position(row, offset)) {
      return offset - start_index;
    }
  }

  if (first_char) {
    return row->len - start_index;  // Return the offset to the end of the line
  }
  return 0;
}

int buffer_row_get_offset_to_prev_word(const BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index > row->len) {
    return 0;  // Invalid row or start index
  }
  bool first_char = false;

  for (int i = start_index - 1; i > 0; i--) {
    if (!first_char) {
      if (!buffer_row_has_whitespace_at_position(row, i)) {
        first_char = true;  // Found the first non-whitespace character
      } else {
        continue;
      }
    }

    if (first_char && buffer_row_has_whitespace_at_position(row, i)) {
      return i - start_index + 1;  // Return the offset to the next word
    }
  }

  if (first_char) {
    return -start_index;  // Return the offset to the start of the line
  }

  return 0;
}

void buffer_row_replace_line(BufferRow* row, const char* new_line) {
  if (row == NULL || new_line == NULL) {
    return;  // Invalid row or new line
  }

  row->len = strlen(new_line);
  row->allocated_size = row->len + 1;  // +1 for null terminator
  row->data = realloc(row->data, row->allocated_size);
  row->highlight_data = realloc(row->highlight_data, row->allocated_size);
  if (row->data != NULL) {
    strcpy(row->data, new_line);
    row->data[row->len] = '\0';  // Null-terminate the string
  }
  row->dirty = true;
  buffer_row_highlight_line(row);
}

int buffer_row_remove_chars(BufferRow* row, int index, int number) {
  if (row == NULL || index < 0 || index >= row->len) {
    return 0;
  }
  if (number + index > row->len) {
    number = row->len - index;
  }

  memmove(&row->data[index], &row->data[index + number], row->len - index - number);
  row->len -= number;
  row->data[row->len] = '\0';
  row->dirty = true;
  buffer_row_highlight_line(row);
  return number;
}

bool buffer_row_remove_char(BufferRow* row, int index) {
  return buffer_row_remove_chars(row, index, 1);
}

void buffer_row_insert_chars(BufferRow* row,
                             int index,
                             const char* str,
                             int number) {
  if (row == NULL || index < 0) {
    return;  // Invalid row or index
  }

  // reallocation is optimized to reduce realloc overhead
  if (row->len + number >= row->allocated_size) {
    // Reallocate memory if needed
    row->allocated_size = (row->len + number) << 1;
    row->data = realloc(row->data, row->allocated_size);
    row->highlight_data = realloc(row->highlight_data, row->allocated_size);
  }

  if (row->data == NULL) {
    return;  // Memory allocation failed
  }

  memmove(&row->data[index + number], &row->data[index], row->len - index + 1);
  memcpy(&row->data[index], str, number);
  row->len += number;
  row->dirty = true;
  buffer_row_highlight_line(row);
}

void buffer_row_insert_char(BufferRow* row, int index, char c) {
  buffer_row_insert_chars(row, index, &c, 1);
}

void buffer_row_append_char(BufferRow* row, char c) {
  if (row == NULL) {
    return;  // Invalid row
  }
  buffer_row_insert_char(row, row->len, c);
}

void buffer_row_append_str(BufferRow* row, const char* str, int number) {
  if (row == NULL) {
    return;  // Invalid row
  }
  buffer_row_insert_chars(row, row->len, str, number);
}

void buffer_row_trim(BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index >= row->len) {
    return;  // Invalid row or start index
  }

  row->len = start_index;
  row->data[row->len] = '\0';  // Null-terminate the string
  row->dirty = true;
  buffer_row_highlight_line(row);
}

BufferRow* buffer_row_get_next(const BufferRow* row) {
  if (row == NULL) {
    return NULL;
  }
  return row->next;
}

BufferRow* buffer_row_get_prev(const BufferRow* row) {
  if (row == NULL) {
    return NULL;
  }
  return row->prev;
}

void buffer_row_mark_dirty(BufferRow* row) {
  if (row)
    row->dirty = true;  // Mark the row as dirty
}

void buffer_row_set_highlight(BufferRow* row,
                              int column_start,
                              int column_end,
                              EHighlightToken token) {
  if (row == NULL || column_start < 0 || column_end < column_start ||
      column_end > row->len) {
    return;  // Invalid row or indices
  }

  for (int i = column_start; i < column_end; ++i) {
    row->highlight_data[i] = (char)token;
  }
}