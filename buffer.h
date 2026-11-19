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

// Forward declaration
struct Filetype;

// How this file is indented when :set said so for it (editor.c); Default
// follows the filetype, the file's own indentation, then the settings.
typedef enum {
  BufferIndent_Default = 0,
  BufferIndent_Tabs,
  BufferIndent_Spaces,
} BufferIndent;

typedef struct Buffer {
  BufferRow* head;
  BufferRow* tail;
  BufferRow* current_row;
  int number_of_rows;
  char* filename;
  const struct Filetype* filetype;  // NULL for plain text

  // Edit tracking: `changes` counts edits; the buffer is modified while it
  // differs from `saved_changes` (taken at load and at :w) and needs an
  // autosave while it differs from `autosaved_changes`.
  unsigned long changes;
  unsigned long saved_changes;
  unsigned long autosaved_changes;
  // When `changes` was last seen to move, for the idle autosave (editor.c).
  unsigned long observed_changes;
  long long observed_at_ms;
  char* autosave_path;   // this session's autosave file, NULL until written
  char* stale_autosave;  // left by a crash, until :recover or :delautosave
  bool autosave_disabled;  // :help and other buffers that are not files
  BufferIndent indent;     // :set expandtab / noexpandtab in this file
  int shiftwidth;          // :set shiftwidth in this file, 0 while not set
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
int buffer_join_current_line_with_previous(Buffer* buffer);

const char* buffer_get_filename(const Buffer* buffer);
int buffer_get_current_line_number(const Buffer* buffer);
bool buffer_is_modified(const Buffer* buffer);
// Every edit of the text calls this; it drives the [+] mark and autosave.
void buffer_note_change(Buffer* buffer);
// The text now equals the file on disk (it was just loaded or written).
void buffer_mark_saved(Buffer* buffer);
