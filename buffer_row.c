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

static const char* symbols = "+-*/{}<>=";
static const char* whitespace_symbols = " \f\n\r\t\v";
static const char* include_symbols = "\"<>";
static const char* string_symbols = "\"'";

void buffer_row_highlight_line(BufferRow* row) {
  if (row == NULL) {
    return;  // Invalid row
  }
  int preprocessor_started = 0;
  int include_started = 0;
  int string_started = 0;
  int multiline_string = 0;
  for (int i = 0; i < row->len; ++i) {
    if (string_started || multiline_string) {
      row->highlight_data[i] = (char)EHighlightToken_String;
      if (row->data[i] == '\\' && (multiline_string == 0)) {
        multiline_string = string_started;
        // how to handle lines below?
      } else if (row->data[i] == string_started ||
                 row->data[i] == multiline_string) {
        string_started = 0;
        multiline_string = 0;
      }
    } else if (include_started) {
      if (strspn(&row->data[i], include_symbols) != 0) {
        ++include_started;
      }
      if (include_started == 3) {
        include_started = 0;  // Reset after processing include
      }
      row->highlight_data[i] = (char)EHighlightToken_String;
      continue;
    } else if (preprocessor_started) {
      if (strchr(whitespace_symbols, row->data[i]) != NULL) {
        row->highlight_data[i] = (char)EHighlightToken_Normal;
        if (strstr(&row->data[preprocessor_started], "include") ==
            &row->data[preprocessor_started]) {
          include_started = 1;
        }
        preprocessor_started = 0;
        continue;
      }
      row->highlight_data[i] = (char)EHighlightToken_Preprocessor;
      continue;
    } else if (strchr(string_symbols, row->data[i]) != NULL) {
      if (row->prev != NULL) {
        if (row->prev->data[row->prev->len - 1] == '\\') {
          // that's end of
          for (int i = 0; i < row->len; ++i) {
            row->highlight_data[i] = (char)EHighlightToken_String;
          }
          continue;
        }
      }

      string_started = row->data[i];
      row->highlight_data[i] = (char)EHighlightToken_String;
    } else if (is_digit(row->data[i])) {
      row->highlight_data[i] = (char)EHighlightToken_Digit;
    } else if (strchr(symbols, row->data[i]) != NULL) {
      row->highlight_data[i] = (char)EHighlightToken_Symbol;
      // row->highlight_data[i] = (char)EHighlightToken_Normal;  // Default highlight
    } else if (row->data[i] == '#') {
      preprocessor_started = i + 1;
      row->highlight_data[i] = (char)EHighlightToken_Preprocessor;
    } else {
      row->highlight_data[i] = (char)EHighlightToken_Normal;  // Default highlight
    }
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

bool buffer_row_remove_char(BufferRow* row, int index) {
  if (row == NULL || index < 0 || index >= row->len) {
    return false;  // Invalid row or index
  }

  memmove(&row->data[index], &row->data[index + 1], row->len - index - 1);
  row->len--;
  row->data[row->len] = '\0';  // Null-terminate the string
  row->dirty = true;
  buffer_row_highlight_line(row);
  return true;
}

void buffer_row_insert_char(BufferRow* row, int index, char c) {
  if (row == NULL || index < 0) {
    return;  // Invalid row or index
  }

  // reallocation is optimized to reduce realloc overhead
  if (row->len + 1 >= row->allocated_size) {
    // Reallocate memory if needed
    row->allocated_size = row->len + 16;
    row->data = realloc(row->data, row->allocated_size);
    row->highlight_data = realloc(row->highlight_data, row->allocated_size);
  }

  if (row->data == NULL) {
    return;  // Memory allocation failed
  }

  memmove(&row->data[index + 1], &row->data[index], row->len - index + 1);
  row->data[index] = c;
  row->len++;
  row->dirty = true;
  buffer_row_highlight_line(row);
}

void buffer_row_append_char(BufferRow* row, char c) {
  if (row == NULL) {
    return;  // Invalid row
  }
  buffer_row_insert_char(row, row->len, c);
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