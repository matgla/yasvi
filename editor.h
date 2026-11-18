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

// The C library's tags file (build_rootfs.sh writes it from /usr/include):
// completion and Ctrl-] in C buffers read it after the project's tags.
#ifndef YASVI_SYSTEM_TAGS
#define YASVI_SYSTEM_TAGS "/usr/share/yasvi/libc.tags"
#endif

#include <stdbool.h>

#include "buffer.h"
#include "command.h"
#include "config.h"
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

// Where Ctrl-] / :tag came from, for Ctrl-T to return to.
#define EDITOR_TAG_STACK_DEPTH 20
typedef struct {
  char* filename;
  int line;    // 0-based
  int column;  // 0-based
} TagStackEntry;

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
  TagStackEntry tag_stack[EDITOR_TAG_STACK_DEPTH];
  int tag_stack_size;
  struct Completion* completion;  // Ctrl-N/Ctrl-P state, allocated on first use
  struct MakeState* make;  // :make settings and error list (make.h), on first use
  EditorConfig config;     // ~/.config/vi.conf and :set (config.h)
  struct Recorder* recorder;  // config.record: keys to FILE.rec (record.h)
  struct TermPanel* terminal;  // :term panel (termpanel.h), on first use
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

// ctags navigation (tags file from `ctags -R`)
bool editor_jump_to_tag(Editor* editor, const char* name);  // Ctrl-], :tag
bool editor_pop_tag(Editor* editor);                        // Ctrl-T, :pop

// Autosave (autosave.h): call editor_idle from the main loop; it writes the
// autosave of a buffer whose edits paused AUTOSAVE_IDLE_MS (now_ms from a
// monotonic clock) or piled up to AUTOSAVE_MAX_PENDING.
// True when the screen needs a redraw although no key came (:term output).
bool editor_idle(Editor* editor);
void editor_autosave_tick(Editor* editor, long long now_ms);

// :make (make.h). editor_set_program_path records argv[0], which
// makemode=exec execs to come back. editor_load_config reads the user's
// config file (config.h: ~/.config/vi.conf), then .yasvirc (the working
// directory's, else $HOME's); either may hold any setting :set takes. editor_restore_session is vi -S FILE:
// the buffers and cursors makemode=exec saved, then the build's result.
// editor_make_report reads a finished build's log (status -1: not started).
void editor_set_program_path(Editor* editor, const char* path);
void editor_load_config(Editor* editor);
// One setting as :set takes it (config.h, then make.h). False when it is
// not understood; a comment or blank line is understood.
bool editor_apply_setting(Editor* editor, const char* line);
bool editor_restore_session(Editor* editor, const char* path);
void editor_make_report(Editor* editor, const char* log_path, int status);
