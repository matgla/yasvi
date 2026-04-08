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

#include "buffer.h"
#include "command.h"
#include "cursor.h"
#include "file_manager.h"
#include "search.h"
#include "window.h"

typedef enum {
  EditorState_Running,
  EditorState_CollectingCommand,
  EditorState_ProcessingCommand,
  EditorState_EditMode,
  EditorState_FileManager,
  EditorState_SearchInputForward,   // / - search forward
  EditorState_SearchInputBackward,  // ? - search backward
  EditorState_Exiting,
} EditorState;

typedef struct {
  EditorState state;
  Command command;
  Window window;
  char* error_message;
  Cursor cursor;
  int number_of_line_digits;
  Buffer* current_buffer;
  Buffer** buffers;
  size_t number_of_buffers;
  size_t current_buffer_index;  // Index of current buffer in buffers array
  bool end_line_mode;
  char* status_bar;
  char key_sequence[32];
  int repeat_count;
  int tab_size;
  int start_line;
  int start_column;
  bool string_rendering_ongoing;
  bool multiline_comment_ongoing;
  int key;
  struct Toolbar* toolbar;  // Widget-based bottom toolbar
  FileManager* file_manager;  // File manager sidebar
  int editor_offset_x;        // X offset for editor content (for sidebar)
  SearchState search;         // Search state for / and ? commands
  Command search_buffer;      // Buffer for search pattern input
} Editor;

void editor_process_key(Editor* editor, int key);
bool editor_should_exit(const Editor* editor);
void editor_redraw_screen(Editor* editor);
void editor_init(Editor* editor);
void editor_deinit(Editor* editor);
void editor_load_file(Editor* editor, const char* filename);
void editor_create_new_file(Editor* editor);
int editor_get_cursor_x(const Editor* editor);
void editor_toggle_file_manager(Editor* editor);
void editor_file_manager_select(Editor* editor);

// Buffer/Tab management
void editor_switch_to_next_buffer(Editor* editor);
void editor_switch_to_prev_buffer(Editor* editor);
void editor_switch_to_buffer_by_index(Editor* editor, size_t index);
void editor_close_current_buffer(Editor* editor);
void editor_draw_tab_bar(Editor* editor);