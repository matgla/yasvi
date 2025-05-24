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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

const char whitespace[] = " \f\n\r\t\v";

int buffer_row_get_offset_to_first_char(BufferRow* row, int start_index) {
  if (row == NULL || start_index < 0 || start_index >= row->len) {
    return 0;  // Invalid buffer or start index
  }

  return strspn(&row->data[start_index], whitespace);
}

bool buffer_row_has_whitespace_at_position(BufferRow* row, int position) {
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
  if (row == NULL || start_index < 0 || start_index >= row->len) {
    return -1;  // Invalid row or start index
  }

  return 10;
}