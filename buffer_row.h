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

#include "highlight.h"

typedef struct BufferRow {
  char* data;
  char* highlight_data;
  int len;
  int allocated_size;
  struct BufferRow* next;
  struct BufferRow* prev;
  bool dirty;
  int highlight_comment_open;
  int highlight_string_open;
} BufferRow;

bool buffer_row_has_whitespace_at_position(const BufferRow* row, int position);
int buffer_row_get_length(const BufferRow* row);

int buffer_row_get_offset_to_first_char(const BufferRow* row, int start_index);
int buffer_row_get_offset_to_next_word(const BufferRow* row, int start_index);
int buffer_row_get_offset_to_prev_word(const BufferRow* row, int start_index);

void buffer_row_replace_line(BufferRow* row, const char* new_line);
bool buffer_row_remove_char(BufferRow* row, int index);
int buffer_row_remove_chars(BufferRow* row, int index, int number);
void buffer_row_insert_char(BufferRow* row, int index, char c);
void buffer_row_insert_chars(BufferRow* row, int index, const char* str, int number);
void buffer_row_trim(BufferRow* row, int start_index);
void buffer_row_append_char(BufferRow* row, char c);
void buffer_row_append_str(BufferRow* row, const char* str, int number);
void buffer_row_break_line(BufferRow* row, int index);

BufferRow* buffer_row_get_next(const BufferRow* row);
BufferRow* buffer_row_get_prev(const BufferRow* row);

void buffer_row_mark_dirty(BufferRow* row);

void buffer_row_set_highlight(BufferRow* row,
                              int column_start,
                              int column_end,
                              EHighlightToken token);
void buffer_row_highlight_line(BufferRow* row);
