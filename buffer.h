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

#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct BufferRow {
  char* data;
  int len;
  int allocated_size;
  struct BufferRow* next;
  struct BufferRow* prev;
} BufferRow;

typedef struct Buffer {
  BufferRow* head;
  BufferRow* tail;
  BufferRow* current_row;
  int number_of_rows;
  int start_line;
  int start_column;
  char* filename;
} Buffer;

Buffer* buffer_alloc();
void buffer_free(Buffer* buffer);

void buffer_load_from_file(Buffer* buffer, const char* filename);
BufferRow* buffer_get_first_row(const Buffer* buffer);
BufferRow* buffer_get_row(const Buffer* buffer, int index);
int buffer_get_line_length(const Buffer* buffer, int index);
bool buffer_append_line(Buffer* buffer, const char* line);
void buffer_insert_row_at(Buffer* buffer, BufferRow* row);
void buffer_remove_row(Buffer* buffer, BufferRow* row);

int buffer_row_get_offset_to_first_char(BufferRow* row, int start_index);
bool buffer_row_has_whitespace_at_position(const BufferRow* row, int position);
int buffer_row_get_length(const BufferRow* row);
int buffer_row_get_offset_to_next_word(const BufferRow* row, int start_index);
int buffer_row_get_offset_to_prev_word(const BufferRow* row, int start_index);
void buffer_row_replace_line(BufferRow* row, const char* new_line);
void buffer_row_remove_char(BufferRow* row, int index);
void buffer_row_insert_char(BufferRow* row, int index, char c);
void buffer_row_insert_str(BufferRow* row, int index, const char* str);
void buffer_row_trim(BufferRow* row, int start_index);
void buffer_row_append_char(BufferRow* row, char c);
