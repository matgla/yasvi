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

static void buffer_insert_below_current(Buffer* buffer) {
  BufferRow* new_row = (BufferRow*)malloc(sizeof(BufferRow));
  if (new_row == NULL) {
    return;  // Memory allocation failed
  }
  new_row->len = 0;
  new_row->allocated_size = 16;  // Initial size for the data buffer
  new_row->data = (char*)malloc(new_row->allocated_size);
  new_row->highlight_data = (char*)malloc(new_row->allocated_size);
  new_row->highlight_comment_open = 0;
  new_row->highlight_string_open = 0;
  memset(new_row->highlight_data, 0, new_row->allocated_size);
  new_row->dirty = 1;
  if (new_row->data == NULL || new_row->highlight_data == NULL) {
    if (new_row->data) {
      free(new_row->data);
    }
    if (new_row->highlight_data) {
      free(new_row->highlight_data);
    }
    free(new_row);
    return;  // Memory allocation failed
  }
  new_row->data[0] = '\0';  // Initialize with an empty string

  BufferRow* current = buffer->current_row;
  new_row->next = current->next;
  new_row->prev = current;
  if (current->next != NULL) {
    current->next->prev = new_row;
  } else {
    buffer->tail = new_row;
  }
  current->next = new_row;
  buffer->number_of_rows++;
}

Buffer* buffer_alloc() {
  Buffer* buffer = (Buffer*)malloc(sizeof(Buffer));
  if (buffer) {
    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->current_row = NULL;
    buffer->number_of_rows = 0;
    buffer->filename = NULL;
  }
  return buffer;
}

void buffer_free(Buffer* buffer) {
  if (buffer) {
    BufferRow* current = buffer->head;
    while (current) {
      BufferRow* next = current->next;
      free(current->data);
      free(current->highlight_data);
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
  new_row->allocated_size = new_row->len + 1;
  new_row->data = (char*)malloc(new_row->allocated_size);
  new_row->highlight_data = (char*)malloc(new_row->allocated_size);
  new_row->highlight_comment_open = 0;
  new_row->highlight_string_open = 0;
  if (new_row->data == NULL || new_row->highlight_data == NULL) {
    if (new_row->data) {
      free(new_row->data);
    }
    if (new_row->highlight_data) {
      free(new_row->highlight_data);
    }
    free(new_row);
    return false;  // Memory allocation failed
  }

  strcpy(new_row->data, line);
  if (!buffer_append_list_row(buffer, new_row)) {
    free(new_row->data);
    free(new_row);
    return false;  // Failed to append row to buffer
  }

  for (int i = new_row->len - 1; i >= 0; i--) {
    if (new_row->data[i] != '\n' && new_row->data[i] != '\r') {
      break;  // Stop at the first non-newline character
    }
    new_row->data[i] = '\0';  // Null-terminate the string
    new_row->len--;           // Reduce length for trailing newlines
  }
  new_row->dirty = true;               // Mark the row as dirty
  buffer_row_highlight_line(new_row);  // Highlight the new row

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

BufferRow* buffer_get_current_line(const Buffer* buffer) {
  if (buffer == NULL) {
    return NULL;
  }
  return buffer->current_row;
}

void buffer_load_from_file(Buffer* buffer, const char* filename) {
  if (buffer == NULL || filename == NULL) {
    return;
  }

  FILE* file = fopen(filename, "r");
  buffer->filename = strdup(filename);
  if (file == NULL) {
    buffer_append_line(buffer, "\n");
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

void buffer_remove_row(Buffer* buffer, BufferRow* row) {
  if (buffer == NULL || row == NULL) {
    return;  // Invalid buffer or row
  }

  if (row->prev != NULL) {
    row->prev->next = row->next;
  } else {
    buffer->head = row->next;  // Remove head
  }

  if (row->next != NULL) {
    row->next->prev = row->prev;
  } else {
    buffer->tail = row->prev;  // Remove tail
  }

  free(row->data);
  free(row);
  buffer->number_of_rows--;
}

int buffer_remove_current_row(Buffer* buffer) {
  if (buffer == NULL || buffer->current_row == NULL) {
    return 0;  // Invalid buffer or current row
  }

  BufferRow* row_to_remove = buffer->current_row;
  int row_offset = 0;
  if (row_to_remove->next != NULL) {
    buffer->current_row = row_to_remove->next;
    row_offset = 1;
  } else if (row_to_remove->prev != NULL) {
    buffer->current_row = row_to_remove->prev;
    row_offset = -1;
  } else {
    buffer->current_row = NULL;  // No more rows left
  }

  buffer_remove_row(buffer, row_to_remove);
  return row_offset;
}

void buffer_scroll_rows(Buffer* buffer, int lines) {
  if (buffer == NULL) {
    return;  // Invalid buffer
  }

  if (lines > 0) {
    for (int i = 0; i < lines && buffer->current_row != NULL; i++) {
      if (buffer->current_row->next != NULL) {
        buffer->current_row = buffer->current_row->next;
      } else {
        return;
      }
    }
  } else if (lines < 0) {
    for (int i = 0; i < -lines && buffer->current_row != NULL; i++) {
      if (buffer->current_row->prev != NULL) {
        buffer->current_row = buffer->current_row->prev;
      } else {
        return;
      }
    }
  }
}

void buffer_scroll_to_top(Buffer* buffer) {
  if (buffer == NULL) {
    return;  // Invalid buffer
  }
  buffer->current_row = buffer->head;
}

bool buffer_current_is_first_row(const Buffer* buffer) {
  if (buffer == NULL || buffer->current_row == NULL) {
    return false;  // Invalid buffer or current row
  }
  return buffer->current_row == buffer->head;
}

bool buffer_current_is_last_row(const Buffer* buffer) {
  if (buffer == NULL || buffer->current_row == NULL) {
    return false;  // Invalid buffer or current row
  }
  return buffer->current_row == buffer->tail;
}

int buffer_get_number_of_lines(const Buffer* buffer) {
  if (buffer == NULL) {
    return 0;  // Invalid buffer
  }
  return buffer->number_of_rows;
}

void buffer_break_current_line(Buffer* buffer, int index) {
  BufferRow* current_row = buffer->current_row;
  if (buffer == NULL || current_row == NULL || index < 0) {
    return;  // Invalid buffer or current row
  }

  buffer_insert_below_current(buffer);
  // we have new row, if characters left from index, copy below
  if (buffer_row_get_length(current_row) > index) {
    buffer_row_replace_line(current_row->next, current_row->data + index);
    buffer_row_trim(current_row, index);
  }
}

const char* buffer_get_filename(const Buffer* buffer) {
  if (buffer == NULL) {
    return NULL;  // Invalid buffer
  }
  return buffer->filename;
}
