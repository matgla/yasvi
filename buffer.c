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

#include "buffer.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char whitespace[] = " \f\n\r\t\v";

static bool buffer_append_list_row(Buffer* buffer, BufferRow* new_row) {
  if (buffer == NULL || new_row == NULL) {
    return false;
  }

  // list is empty
  if (buffer->tail == NULL) {
    buffer->head = new_row;
    buffer->tail = new_row;
    buffer->current_row = new_row;
    new_row->next = NULL;
    new_row->prev = NULL;
    buffer->number_of_rows = 1;
    return true;
  } else {
    // append to the end of the list
    buffer->tail->next = new_row;
    new_row->prev = buffer->tail;
    new_row->next = NULL;
    buffer->tail = new_row;
    ++buffer->number_of_rows;
    return true;
  }

  return false;
}

Buffer* buffer_alloc() {
  Buffer* buffer = (Buffer*)malloc(sizeof(Buffer));
  if (buffer) {
    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->current_row = NULL;
    buffer->number_of_rows = 0;
    buffer->start_line = 0;
    buffer->start_column = 0;
  }
  return buffer;
}

void buffer_free(Buffer* buffer) {
  if (buffer) {
    BufferRow* current = buffer->head;
    while (current) {
      BufferRow* next = current->next;
      free(current->data);
      free(current);
      current = next;
    }
    if (buffer->filename) {
      free(buffer->filename);
      buffer->filename = NULL;
    }
    free(buffer);
  }
}

bool buffer_append_line(Buffer* buffer, const char* line) {
  if (buffer == NULL || line == NULL) {
    return false;
  }

  BufferRow* new_row = (BufferRow*)malloc(sizeof(BufferRow));
  if (new_row == NULL) {
    return false;  // Memory allocation failed
  }

  new_row->len = strlen(line);
  new_row->data = (char*)malloc(new_row->len + 1);
  new_row->allocated_size = new_row->len + 1;  // +1 for null terminator
  if (new_row->data == NULL) {
    free(new_row);
    return false;  // Memory allocation failed
  }

  strcpy(new_row->data, line);
  if (!buffer_append_list_row(buffer, new_row)) {
    free(new_row->data);
    free(new_row);
    return false;  // Failed to append row to buffer
  }

  if (new_row->data[new_row->len - 1] == '\n' ||
      new_row->data[new_row->len - 1] == '\r') {
    new_row->data[new_row->len - 1] = '\0';
    new_row->len--;
  }

  return true;
}

BufferRow* buffer_get_first_row(const Buffer* buffer) {
  if (buffer == NULL) {
    return NULL;
  }
  return buffer->head;
}

BufferRow* buffer_get_row(const Buffer* buffer, int index) {
  if (buffer == NULL || index < 0 || index >= buffer->number_of_rows) {
    return NULL;
  }

  BufferRow* current = buffer->head;
  for (int i = 0; i < index && current != NULL; i++) {
    current = current->next;
  }
  return current;
}

void buffer_load_from_file(Buffer* buffer, const char* filename) {
  if (buffer == NULL || filename == NULL) {
    return;
  }
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    return;
  }

  char* line = NULL;
  size_t len = 0;

  while (getline(&line, &len, file) != EOF) {
    if (line != NULL) {
      buffer_append_line(buffer, line);
      free(line);
      line = NULL;
    }
  }
  if (line) {
    free(line);
    line = NULL;
  }

  fclose(file);
  buffer->filename = strdup(filename);
}

int buffer_get_line_length(const Buffer* buffer, int index) {
  if (buffer == NULL || index < 0 || index >= buffer->number_of_rows) {
    return -1;  // Invalid index
  }

  BufferRow* row = buffer_get_row(buffer, index);
  if (row == NULL) {
    return -1;  // Row not found
  }
  return row->len;  // Exclude the newline character
}

void buffer_insert_row_at(Buffer* buffer, BufferRow* row) {
  BufferRow* new_row = (BufferRow*)malloc(sizeof(BufferRow));
  if (new_row == NULL) {
    return;  // Memory allocation failed
  }
  new_row->data = (char*)malloc(16);  // Allocate space for an empty string
  if (new_row->data == NULL) {
    free(new_row);
    return;  // Memory allocation failed
  }
  new_row->data[0] = '\0';  // Initialize with an empty string
  new_row->len = 0;
  new_row->allocated_size = 16;  // Initial size for the data buffer

  if (row == NULL) {
    // append to start
    new_row->prev = NULL;
    new_row->next = buffer->head;
    buffer->head->prev = new_row;
    buffer->head = new_row;
  } else if (row->next == NULL) {
    // append to end
    row->next = new_row;
    new_row->prev = row;
    new_row->next = NULL;
  } else {
    new_row->next = row->next;
    row->next->prev = new_row;
    new_row->prev = row;
    row->next = new_row;
  }

  buffer->number_of_rows++;
}

int buffer_row_get_offset_to_first_char(BufferRow* row, int start_index) {
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
  return row->len - 1;  // Return the length of the row
}

int buffer_row_get_offset_to_next_word(const BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index > row->len) {
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
  row->data = realloc(row->data, row->len + 1);
  if (row->data != NULL) {
    strcpy(row->data, new_line);
    row->data[row->len] = '\0';  // Null-terminate the string
  }
}

void buffer_row_remove_char(BufferRow* row, int index) {
  if (row == NULL || index < 0 || index >= row->len) {
    return;  // Invalid row or index
  }

  memmove(&row->data[index], &row->data[index + 1], row->len - index);
  row->len--;
  row->data[row->len] = '\0';  // Null-terminate the string
}

void buffer_row_insert_char(BufferRow* row, int index, char c) {
  if (row == NULL || index < 0 || index > row->len) {
    return;  // Invalid row or index
  }

  // reallocation is optimized to reduce realloc overhead
  if (row->len + 1 >= row->allocated_size) {
    // Reallocate memory if needed
    row->allocated_size = row->len + 16;
    row->data = realloc(row->data, row->allocated_size);
  }

  if (row->data == NULL) {
    return;  // Memory allocation failed
  }

  memmove(&row->data[index + 1], &row->data[index], row->len - index + 1);
  row->data[index] = c;
  row->len++;
}

void buffer_row_append_char(BufferRow* row, char c) {
  if (row == NULL) {
    return;  // Invalid row
  }
  buffer_row_insert_char(row, row->len, 0);
}

void buffer_row_insert_str(BufferRow* row, int index, const char* str) {
  if (row == NULL || str == NULL || index < 0 || index > row->len) {
    return;  // Invalid row, string, or index
  }

  int str_len = strlen(str);
  if (str_len == 0) {
    return;  // Nothing to insert
  }

  // reallocation is optimized to reduce realloc overhead
  if (row->len + str_len >= row->allocated_size) {
    // Reallocate memory if needed
    row->allocated_size = row->len + str_len + 16;
    row->data = realloc(row->data, row->allocated_size);
  }

  if (row->data == NULL) {
    return;  // Memory allocation failed
  }

  memmove(&row->data[index + str_len], &row->data[index], row->len - index + 1);
  memcpy(&row->data[index], str, str_len);
  row->len += str_len;
}

void buffer_row_trim(BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index >= row->len) {
    return;  // Invalid row or start index
  }

  row->len = start_index;
  row->data[row->len] = '\0';  // Null-terminate the string
}