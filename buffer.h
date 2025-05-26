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

#include "buffer_row.h"

typedef struct Buffer {
  BufferRow* head;
  BufferRow* tail;
  BufferRow* current_row;
  int number_of_rows;
  char* filename;
} Buffer;

Buffer* buffer_alloc();
void buffer_free(Buffer* buffer);

// buffer getter functions
BufferRow* buffer_get_first_row(const Buffer* buffer);
BufferRow* buffer_get_row(const Buffer* buffer, int index);
BufferRow* buffer_get_current_line(const Buffer* buffer);

void buffer_load_from_file(Buffer* buffer, const char* filename);
bool buffer_append_line(Buffer* buffer, const char* line);

void buffer_remove_row(Buffer* buffer, BufferRow* row);

// result:
// +1 - next row is now current
// -1 - previous row is now current
// 0 - current row is now NULL
int buffer_remove_current_row(Buffer* buffer);

// buffer scroll functions
// +/- lines from the current row
void buffer_scroll_rows(Buffer* buffer, int lines);
void buffer_scroll_to_top(Buffer* buffer);

bool buffer_current_is_first_row(const Buffer* buffer);
bool buffer_current_is_last_row(const Buffer* buffer);
int buffer_get_number_of_lines(const Buffer* buffer);
void buffer_break_current_line(Buffer* buffer, int index);

const char* buffer_get_filename(const Buffer* buffer);
