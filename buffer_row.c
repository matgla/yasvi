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

#include "filetypes/filetype.h"

const char whitespace[] = " \f\n\r\t\v";

bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

void buffer_row_highlight_line(BufferRow* row) {
  if (row == NULL) {
    return;
  }

  // Inherit state from previous row
  if (row->prev) {
    row->highlight_comment_open = row->prev->highlight_comment_open;
    row->highlight_string_open = row->prev->highlight_string_open;
  }

  row->dirty = true;
  buffer_row_highlight_with_filetype(row, row->filetype);
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

/* Character classification for vim-like word navigation:
 * - Word character: alphanumeric or underscore (iskeyword)
 * - Non-word character: everything else except whitespace
 * - Whitespace: space, tab, etc.
 * 
 * A "word" is either:
 * - A sequence of word characters, OR
 * - A sequence of non-word non-whitespace characters
 */

// Check if character is a "word" character (alphanumeric or underscore)
static bool is_word_char(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         c == '_';
}

// Check if character is whitespace
static bool is_whitespace(char c) {
  return strchr(whitespace, c) != NULL;
}

// Get the word class of a character:
// 0 = whitespace, 1 = word char, 2 = non-word char
static int get_word_class(char c) {
  if (is_whitespace(c)) return 0;
  if (is_word_char(c)) return 1;
  return 2;
}

int buffer_row_get_offset_to_next_word(const BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index >= row->len) {
    return 0;
  }

  int pos = start_index;
  int start_class = get_word_class(row->data[pos]);

  // Case 1: We're on whitespace - skip it and return position of next word
  if (start_class == 0) {
    while (pos < row->len && get_word_class(row->data[pos]) == 0) {
      pos++;
    }
    return pos - start_index;
  }

  // Case 2: We're on a word character - move to end of word
  if (start_class == 1) {
    while (pos < row->len && get_word_class(row->data[pos]) == 1) {
      pos++;
    }
  }
  // Case 3: We're on a special character - move past just this one char
  else {
    pos++;
  }

  // Now we're either at whitespace, different char type, or end of line
  // Skip any whitespace to get to the next word
  while (pos < row->len && get_word_class(row->data[pos]) == 0) {
    pos++;
  }

  return pos - start_index;
}

int buffer_row_get_offset_to_prev_word(const BufferRow* row, int start_index) {
  if (row == NULL || start_index <= 0 || start_index > row->len) {
    return 0;
  }

  int pos = start_index - 1;

  // Step 1: Skip trailing whitespace before the cursor
  while (pos >= 0 && get_word_class(row->data[pos]) == 0) {
    pos--;
  }
  if (pos < 0) {
    return -start_index;  // Move to start of line
  }

  // Remember the class of word we're moving over
  int word_class = get_word_class(row->data[pos]);

  // Step 2: Skip over the current word, or just one char if special
  if (word_class == 1) {
    // Word character - skip the whole word
    while (pos >= 0 && get_word_class(row->data[pos]) == 1) {
      pos--;
    }
  } else {
    // Special character - just move back by 1
    pos--;
  }

  // pos is now at the character before the word/special char, so target is pos + 1
  return (pos + 1) - start_index;
}

int buffer_row_get_offset_to_end_of_word(const BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index >= row->len) {
    return 0;
  }

  int pos = start_index;
  int start_class = get_word_class(row->data[pos]);

  // If we're on whitespace, skip it first
  if (start_class == 0) {
    while (pos < row->len && get_word_class(row->data[pos]) == 0) {
      pos++;
    }
    if (pos >= row->len) {
      return row->len - start_index - 1;
    }
    start_class = get_word_class(row->data[pos]);
  }

  // Move to end of current word/non-word sequence
  while (pos < row->len && get_word_class(row->data[pos]) == start_class) {
    pos++;
  }

  // Move back to last character of the word (if we moved at all)
  if (pos > start_index) {
    pos--;
  }

  return pos - start_index;
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
}

bool buffer_row_remove_char(BufferRow* row, int index) {
  if (row == NULL || index < 0 || index >= row->len) {
    return false;  // Invalid row or index
  }

  // Shift all characters after index one position to the left
  for (int i = index; i < row->len - 1; i++) {
    row->data[i] = row->data[i + 1];
  }
  row->len--;                       // Decrement the length
  row->data[row->len] = '\0';       // Null-terminate the string
  row->data[row->allocated_size - 1] = '\0';  // Ensure null termination
  row->dirty = true;
  return true;  // Character removed successfully
}

int buffer_row_remove_chars(BufferRow* row, int index, int number) {
  if (row == NULL || index < 0 || index >= row->len || number <= 0) {
    return 0;  // Invalid parameters
  }

  int chars_to_remove = number;
  if (index + chars_to_remove > row->len) {
    chars_to_remove = row->len - index;  // Limit to available characters
  }

  // Shift remaining characters
  for (int i = index; i < row->len - chars_to_remove; i++) {
    row->data[i] = row->data[i + chars_to_remove];
  }

  row->len -= chars_to_remove;
  row->data[row->len] = '\0';
  row->dirty = true;

  return chars_to_remove;
}

void buffer_row_insert_char(BufferRow* row, int index, char c) {
  if (row == NULL || index < 0 || index > row->len) {
    return;  // Invalid row or index
  }

  // Ensure there's enough space
  if (row->len + 1 >= row->allocated_size) {
    row->allocated_size *= 2;
    row->data = realloc(row->data, row->allocated_size);
    row->highlight_data = realloc(row->highlight_data, row->allocated_size);
  }

  // Shift characters to the right
  for (int i = row->len; i > index; i--) {
    row->data[i] = row->data[i - 1];
  }

  row->data[index] = c;
  row->len++;
  row->data[row->len] = '\0';
  row->dirty = true;
}

void buffer_row_insert_chars(BufferRow* row,
                             int index,
                             const char* str,
                             int number) {
  if (row == NULL || str == NULL || index < 0 || index > row->len ||
      number <= 0) {
    return;
  }

  // Ensure there's enough space
  while (row->len + number >= row->allocated_size) {
    row->allocated_size *= 2;
    row->data = realloc(row->data, row->allocated_size);
    row->highlight_data = realloc(row->highlight_data, row->allocated_size);
  }

  // Shift characters to the right
  for (int i = row->len + number - 1; i >= index + number; i--) {
    row->data[i] = row->data[i - number];
  }

  // Insert the new characters
  for (int i = 0; i < number; i++) {
    row->data[index + i] = str[i];
  }

  row->len += number;
  row->data[row->len] = '\0';
  row->dirty = true;
}

void buffer_row_trim(BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index >= row->len) {
    return;  // Invalid row or start index
  }

  // Remove characters from start_index to end
  row->len = start_index;
  row->data[row->len] = '\0';
  row->dirty = true;
}

void buffer_row_append_char(BufferRow* row, char c) {
  if (row == NULL) {
    return;  // Invalid row
  }

  // Ensure there's enough space
  if (row->len + 1 >= row->allocated_size) {
    row->allocated_size *= 2;
    row->data = realloc(row->data, row->allocated_size);
    row->highlight_data = realloc(row->highlight_data, row->allocated_size);
  }

  row->data[row->len] = c;
  row->len++;
  row->data[row->len] = '\0';
  row->dirty = true;
}

void buffer_row_append_str(BufferRow* row, const char* str, int number) {
  if (row == NULL || str == NULL || number <= 0) {
    return;
  }

  // Ensure there's enough space
  while (row->len + number >= row->allocated_size) {
    row->allocated_size *= 2;
    row->data = realloc(row->data, row->allocated_size);
    row->highlight_data = realloc(row->highlight_data, row->allocated_size);
  }

  // Append the string
  for (int i = 0; i < number; i++) {
    row->data[row->len + i] = str[i];
  }

  row->len += number;
  row->data[row->len] = '\0';
  row->dirty = true;
}

void buffer_row_break_line(BufferRow* row, int index) {
  if (row == NULL || index < 0 || index > row->len) {
    return;
  }

  // Create a new row with the content after the break point
  BufferRow* new_row = (BufferRow*)malloc(sizeof(BufferRow));
  if (new_row == NULL) {
    return;
  }

  int new_len = row->len - index;
  new_row->len = new_len;
  new_row->allocated_size = new_len + 1;
  new_row->data = (char*)malloc(new_row->allocated_size);
  new_row->highlight_data = (char*)malloc(new_row->allocated_size);
  new_row->dirty = true;
  new_row->highlight_comment_open = row->highlight_comment_open;
  new_row->highlight_string_open = row->highlight_string_open;
  new_row->filetype = row->filetype;

  if (new_row->data != NULL) {
    memcpy(new_row->data, row->data + index, new_len);
    new_row->data[new_len] = '\0';
  }

  // Truncate the current row
  row->len = index;
  row->data[index] = '\0';
  row->dirty = true;

  // Insert the new row after the current row
  new_row->next = row->next;
  new_row->prev = row;
  if (row->next != NULL) {
    row->next->prev = new_row;
  }
  row->next = new_row;
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
  if (row != NULL) {
    row->dirty = true;
  }
}

void buffer_row_set_highlight(BufferRow* row,
                              int column_start,
                              int column_end,
                              EHighlightToken token) {
  if (row == NULL || row->highlight_data == NULL) {
    return;
  }

  if (column_start < 0) {
    column_start = 0;
  }
  if (column_end > row->len) {
    column_end = row->len;
  }

  for (int i = column_start; i < column_end; i++) {
    row->highlight_data[i] = token;
  }
}
