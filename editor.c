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

#include "editor.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <ncurses.h>

#include "command.h"
#include "highlight.h"
#include "file_manager.h"
#include "search.h"
#include "toolbar.h"
#include "toolbar_widgets.h"

#define EDITOR_TOP_BAR_HEIGHT 2
// 1 is for tab bar
// 1 is for separator line
// 1 is for command line
// 1 is for status bar
#define EDITOR_BOTTOM_BAR_HEIGHT 3

typedef enum {
  CommandResult_Success = 0,
  CommandResult_CommandNotFound = 1,
  CommandResult_ShouldExit = 2,
} CommandResult;

static void editor_move_cursor_to_end(Editor* editor);
static void editor_move_cursor_x(Editor* editor, int x, bool insert_mode);
static void editor_move_cursor_y(Editor* editor, int y);
static bool editor_process_key_sequence(Editor* editor, int key);
static void editor_set_error_message(Editor* editor, const char* message);
static void editor_home_cursor_x(Editor* editor);
static void editor_home_cursor_y(Editor* editor);
static void editor_home_cursor_xy(Editor* editor);
static void editor_fix_cursor_position(Editor* editor);
static int editor_get_content_offset_x(const Editor* editor);

#ifdef __GNUC__
char* itoa(int n, char* s, int base) {
  char* p = s;
  int sign = n < 0 ? -1 : 1;
  if (sign < 0)
    n = -n;
  do {
    *p++ = "0123456789abcdef"[n % base];
    n /= base;
  } while (n);
  if (sign < 0)
    *p++ = '-';
  *p-- = '\0';
  while (s < p) {
    char tmp = *s;
    *s++ = *p;
    *p-- = tmp;
  }
  return s;
}
#endif

int editor_get_cursor_x(const Editor* editor) {
  int content_offset = editor_get_content_offset_x(editor);
  int line_digit_width = 4;  // Base line number width
  if (editor->cursor.x + editor->start_column < content_offset + line_digit_width) {
    return 0;
  }
  return editor->cursor.x + editor->start_column - content_offset - line_digit_width;
}

static void editor_home_cursor_x(Editor* editor) {
  editor->cursor.x = editor->number_of_line_digits;
  editor->start_column = 0;
}

static void editor_mark_dirty_whole_screen(Editor* editor) {
  const int number_of_lines =
    editor->window.height - EDITOR_TOP_BAR_HEIGHT - EDITOR_BOTTOM_BAR_HEIGHT;
  BufferRow* row = buffer_get_row(editor->current_buffer, editor->start_line);
  if (row == NULL) {
    return;  // No rows in the buffer
  }
  for (int y = 0; y < number_of_lines; ++y) {
    buffer_row_mark_dirty(row);
    if (row->next != NULL) {
      row = row->next;
    } else {
      break;  // No more rows to mark
    }
  }
}

static void editor_mark_dirty_from_cursor(Editor* editor) {
  BufferRow* row = buffer_get_row(editor->current_buffer, editor->start_line);
  if (row == NULL) {
    return;  // No rows in the buffer
  }
  for (int y = 0; y < editor->window.height; ++y) {
    if (row != NULL) {
      buffer_row_mark_dirty(row);
      row = row->next;
    } else {
      break;  // No more rows to mark
    }
  }
}

static void editor_home_cursor_y(Editor* editor) {
  editor->cursor.y = EDITOR_TOP_BAR_HEIGHT;
  editor->start_line = 0;
  editor_mark_dirty_whole_screen(editor);
}

static void editor_home_cursor_xy(Editor* editor) {
  editor_home_cursor_x(editor);
  editor_home_cursor_y(editor);
}

// true if editor loop should continue
static bool editor_collect_command(Editor* editor, int key) {
  if (key == '\n') {
    editor->state = EditorState_ProcessingCommand;
    return true;
  } else if (key == 27) {
    // cancel command
    command_deinit(&editor->command);
    editor->state = EditorState_Running;
    return true;
  } else if (key == KEY_BACKSPACE || key == 127) {
    editor->command.cursor_position--;
    editor->command.buffer[editor->command.cursor_position] = '\0';
    return false;
  }
  command_append(&editor->command, (char)key);
  return false;
}

static void editor_clear_error_message(Editor* editor) {
  if (editor->error_message) {
    for (size_t i = 0; i < strlen(editor->error_message); ++i) {
      mvaddch(editor->window.height - 1, 1 + i, ' ');
    }
    free(editor->error_message);
    editor->error_message = NULL;
  }
}

// Handle search pattern input (for / and ? commands)
// Returns true if search should continue to next state
static bool editor_collect_search_pattern(Editor* editor, int key) {
  if (key == '\n') {
    // Execute search
    if (editor->search_buffer.buffer != NULL &&
        strlen(editor->search_buffer.buffer) > 0) {
      search_set_pattern(&editor->search, editor->search_buffer.buffer,
                         editor->state == EditorState_SearchInputForward);
      search_set_position(&editor->search,
                          buffer_get_current_line_number(editor->current_buffer) -
                            1,
                          editor_get_cursor_x(editor));

      bool found;
      if (editor->state == EditorState_SearchInputForward) {
        found = search_find_next(&editor->search, editor->current_buffer);
      } else {
        found = search_find_prev(&editor->search, editor->current_buffer);
      }

      if (found) {
        // Move cursor to match
        int target_line = editor->search.last_match_line;
        int target_col = editor->search.last_match_col;
        int current_line =
          buffer_get_current_line_number(editor->current_buffer) - 1;

        // Scroll to the line
        int line_diff = target_line - current_line;
        if (line_diff != 0) {
          buffer_scroll_rows(editor->current_buffer, line_diff);
          editor->start_line = target_line;
          editor->cursor.y = EDITOR_TOP_BAR_HEIGHT;
          editor_mark_dirty_whole_screen(editor);
        }

        // Move cursor to column
        editor_home_cursor_x(editor);
        editor_move_cursor_x(editor, target_col, false);
      } else {
        editor_set_error_message(editor, "Pattern not found");
      }
    }
    command_deinit(&editor->search_buffer);
    editor->state = EditorState_Running;
    return true;
  } else if (key == 27) {
    // Cancel search
    command_deinit(&editor->search_buffer);
    editor->state = EditorState_Running;
    return true;
  } else if (key == KEY_BACKSPACE || key == 127) {
    if (editor->search_buffer.cursor_position > 0) {
      editor->search_buffer.cursor_position--;
      editor->search_buffer.buffer[editor->search_buffer.cursor_position] =
        '\0';
    }
    return false;
  }
  command_append(&editor->search_buffer, (char)key);
  return false;
}

// Jump to next search match
static void editor_search_next(Editor* editor) {
  if (editor->search.pattern == NULL) {
    return;
  }

  // Do NOT update position from cursor - continue from last match
  // The search state already tracks where we are
  bool found = search_find_next(&editor->search, editor->current_buffer);

  if (found) {
    int target_line = editor->search.last_match_line;
    int target_col = editor->search.last_match_col;
    int current_line =
      buffer_get_current_line_number(editor->current_buffer) - 1;

    int line_diff = target_line - current_line;
    if (line_diff != 0) {
      buffer_scroll_rows(editor->current_buffer, line_diff);
      editor->start_line = target_line;
      editor->cursor.y = EDITOR_TOP_BAR_HEIGHT;
      editor_mark_dirty_whole_screen(editor);
    }

    editor_home_cursor_x(editor);
    editor_move_cursor_x(editor, target_col, false);
  } else {
    editor_set_error_message(editor, "Pattern not found");
  }
}

// Jump to previous search match
static void editor_search_prev(Editor* editor) {
  if (editor->search.pattern == NULL) {
    return;
  }

  // Do NOT update position from cursor - continue from last match
  // The search state already tracks where we are
  bool found = search_find_prev(&editor->search, editor->current_buffer);

  if (found) {
    int target_line = editor->search.last_match_line;
    int target_col = editor->search.last_match_col;
    int current_line =
      buffer_get_current_line_number(editor->current_buffer) - 1;

    int line_diff = target_line - current_line;
    if (line_diff != 0) {
      buffer_scroll_rows(editor->current_buffer, line_diff);
      editor->start_line = target_line;
      editor->cursor.y = EDITOR_TOP_BAR_HEIGHT;
      editor_mark_dirty_whole_screen(editor);
    }

    editor_home_cursor_x(editor);
    editor_move_cursor_x(editor, target_col, false);
  } else {
    editor_set_error_message(editor, "Pattern not found");
  }
}

static CommandResult editor_process_save_command(Editor* editor) {
  int command_length = strlen(editor->command.buffer);
  const char* filename = buffer_get_filename(editor->current_buffer);
  bool should_exit = false;
  if (command_length > 1) {
    // additional arguments for the command
    if (editor->command.buffer[1] == ' ') {
      // save to a different file
      filename = &editor->command.buffer[2];
      while (isspace(*filename)) {
        filename++;  // Skip leading spaces
      }
      if (strlen(filename) == 0) {
        filename = buffer_get_filename(editor->current_buffer);
      }
    } else if (editor->command.buffer[1] == 'q') {
      // force save
      should_exit = true;
    } else {
      editor_set_error_message(editor, "Invalid command syntax");
      return CommandResult_CommandNotFound;
    }
  }

  if (filename == NULL || strlen(filename) == 0) {
    editor_set_error_message(editor, "No filename specified for saving");
    return CommandResult_CommandNotFound;
  }
  FILE* file = fopen(filename, "w");

  if (file == NULL) {
    editor_set_error_message(editor, "Failed to open file for writing");
    return CommandResult_CommandNotFound;
  }

  for (BufferRow* row = buffer_get_first_row(editor->current_buffer); row != NULL;
       row = row->next) {
    if (row->data) {
      fprintf(file, "%s\n", row->data);
    }
  }

  fclose(file);
  editor_set_error_message(editor, "File saved successfully");
  return should_exit ? CommandResult_ShouldExit : CommandResult_Success;
}

static CommandResult editor_process_command(Editor* editor) {
  const Command* command = &editor->command;
  if (command->buffer == NULL) {
    return CommandResult_CommandNotFound;
  }
  if (strcmp(command->buffer, "q") == 0) {
    return CommandResult_ShouldExit;
  }

  if (command->buffer[0] == 'w') {
    return editor_process_save_command(editor);
  }

  if (strcmp(command->buffer, "bd") == 0 || strcmp(command->buffer, "bdelete") == 0) {
    editor_close_current_buffer(editor);
    return CommandResult_Success;
  }

  // Handle :bN where N is buffer index (1-indexed for user)
  if (command->buffer[0] == 'b' && command->buffer[1] >= '1' && command->buffer[1] <= '9') {
    int target = atoi(&command->buffer[1]) - 1;  // Convert to 0-indexed
    if (target >= 0 && (size_t)target < editor->number_of_buffers) {
      editor_switch_to_buffer_by_index(editor, (size_t)target);
      return CommandResult_Success;
    }
    editor_set_error_message(editor, "Invalid buffer number");
    return CommandResult_CommandNotFound;
  }

  // Handle :bn (next buffer) and :bp (previous buffer)
  if (strcmp(command->buffer, "bn") == 0) {
    editor_switch_to_next_buffer(editor);
    return CommandResult_Success;
  }
  if (strcmp(command->buffer, "bp") == 0) {
    editor_switch_to_prev_buffer(editor);
    return CommandResult_Success;
  }

  // Handle substitution commands: :s/old/new/flags
  if (command->buffer[0] == 's') {
    // Parse substitution pattern: s/pattern/replacement/flags
    const char* cmd = command->buffer + 1;  // Skip 's'
    if (*cmd != '/') {
      editor_set_error_message(editor, "Invalid substitute syntax");
      return CommandResult_CommandNotFound;
    }
    cmd++;  // Skip first '/'

    // Find pattern end
    const char* pattern_start = cmd;
    const char* pattern_end = strchr(cmd, '/');
    if (pattern_end == NULL) {
      editor_set_error_message(editor, "Invalid substitute syntax");
      return CommandResult_CommandNotFound;
    }

    size_t pattern_len = pattern_end - pattern_start;
    char* pattern = (char*)malloc(pattern_len + 1);
    if (pattern == NULL) {
      editor_set_error_message(editor, "Memory allocation failed");
      return CommandResult_CommandNotFound;
    }
    strncpy(pattern, pattern_start, pattern_len);
    pattern[pattern_len] = '\0';

    // Find replacement end
    const char* replacement_start = pattern_end + 1;
    const char* replacement_end = strchr(replacement_start, '/');
    if (replacement_end == NULL) {
      replacement_end = replacement_start + strlen(replacement_start);
    }

    size_t replacement_len = replacement_end - replacement_start;
    char* replacement = (char*)malloc(replacement_len + 1);
    if (replacement == NULL) {
      free(pattern);
      editor_set_error_message(editor, "Memory allocation failed");
      return CommandResult_CommandNotFound;
    }
    strncpy(replacement, replacement_start, replacement_len);
    replacement[replacement_len] = '\0';

    // Check for flags
    bool global = false;
    if (replacement_end != NULL && *(replacement_end + 1) != '\0') {
      const char* flags = replacement_end + 1;
      if (strchr(flags, 'g') != NULL) {
        global = true;
      }
    }

    // Execute substitution on current line
    int current_line =
      buffer_get_current_line_number(editor->current_buffer) - 1;
    ReplaceResult result = search_replace_line(
      editor->current_buffer, current_line, pattern, replacement, global);

    if (result.replacements > 0) {
      static char msg[64];
      snprintf(msg, sizeof(msg), "Replaced %d occurrence(s)",
               result.replacements);
      editor_set_error_message(editor, msg);
      editor_mark_dirty_from_cursor(editor);
    } else {
      editor_set_error_message(editor, "Pattern not found");
    }

    free(pattern);
    free(replacement);
    return CommandResult_Success;
  }

  // Handle %s substitution: :%s/old/new/g
  if (command->buffer[0] == '%' && command->buffer[1] == 's') {
    const char* cmd = command->buffer + 2;  // Skip '%s'
    if (*cmd != '/') {
      editor_set_error_message(editor, "Invalid substitute syntax");
      return CommandResult_CommandNotFound;
    }
    cmd++;  // Skip first '/'

    // Find pattern end
    const char* pattern_start = cmd;
    const char* pattern_end = strchr(cmd, '/');
    if (pattern_end == NULL) {
      editor_set_error_message(editor, "Invalid substitute syntax");
      return CommandResult_CommandNotFound;
    }

    size_t pattern_len = pattern_end - pattern_start;
    char* pattern = (char*)malloc(pattern_len + 1);
    if (pattern == NULL) {
      editor_set_error_message(editor, "Memory allocation failed");
      return CommandResult_CommandNotFound;
    }
    strncpy(pattern, pattern_start, pattern_len);
    pattern[pattern_len] = '\0';

    // Find replacement end
    const char* replacement_start = pattern_end + 1;
    const char* replacement_end = strchr(replacement_start, '/');
    if (replacement_end == NULL) {
      replacement_end = replacement_start + strlen(replacement_start);
    }

    size_t replacement_len = replacement_end - replacement_start;
    char* replacement = (char*)malloc(replacement_len + 1);
    if (replacement == NULL) {
      free(pattern);
      editor_set_error_message(editor, "Memory allocation failed");
      return CommandResult_CommandNotFound;
    }
    strncpy(replacement, replacement_start, replacement_len);
    replacement[replacement_len] = '\0';

    // Check for flags
    bool global = false;
    if (replacement_end != NULL && *(replacement_end + 1) != '\0') {
      const char* flags = replacement_end + 1;
      if (strchr(flags, 'g') != NULL) {
        global = true;
      }
    }

    // Execute substitution on entire buffer
    ReplaceResult result = search_replace_all(
      editor->current_buffer, pattern, replacement, global);

    if (result.replacements > 0) {
      static char msg[64];
      snprintf(msg, sizeof(msg), "Replaced %d occurrence(s) in %d line(s)",
               result.replacements, result.lines_affected);
      editor_set_error_message(editor, msg);
      editor_mark_dirty_whole_screen(editor);
    } else {
      editor_set_error_message(editor, "Pattern not found");
    }

    free(pattern);
    free(replacement);
    return CommandResult_Success;
  }

  return CommandResult_CommandNotFound;
}

static void editor_set_error_message(Editor* editor, const char* message) {
  int error_length = strlen(message);
  int message_offset = 0;
  editor_clear_error_message(editor);
  editor->error_message =
    (char*)malloc(editor->command.cursor_position + error_length + 5);
  if (editor->error_message && editor->command.buffer) {
    memcpy(editor->error_message, message, error_length);
    message_offset += error_length;
    memcpy(editor->error_message + message_offset, ": '", 3);
    message_offset += 3;
    memcpy(editor->error_message + message_offset, editor->command.buffer,
           editor->command.cursor_position);
    message_offset += editor->command.cursor_position;
    editor->error_message[message_offset] = '\'';
    ++message_offset;
    editor->error_message[message_offset] = '\0';
  } else if (editor->error_message != NULL) {
    memcpy(editor->error_message, message, error_length);
    editor->error_message[error_length] = '\0';
  }
  editor_redraw_screen(editor);
}

static void editor_restore_cursor_position(const Editor* editor) {
  move(editor->cursor.y, editor->cursor.x);
}

static void editor_move_cursor_to_start(Editor* editor) {
  const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  editor_home_cursor_x(editor);
  editor_move_cursor_x(editor, buffer_row_get_offset_to_first_char(current_row, 0),
                       false);
}

static void editor_move_to_top(Editor* editor) {
  if (buffer_current_is_first_row(editor->current_buffer)) {
    return;  // Already at the first row
  }
  buffer_scroll_to_top(editor->current_buffer);
  editor_home_cursor_xy(editor);
}

static void editor_move_to_bottom(Editor* editor) {
  const int number_of_lines = buffer_get_number_of_lines(editor->current_buffer);
  const int lines_to_the_end = number_of_lines - editor->start_line - 1;
  if (buffer_current_is_last_row(editor->current_buffer)) {
    return;  // Already at the last row
  }
  editor_move_cursor_y(editor, lines_to_the_end);
  buffer_scroll_rows(editor->current_buffer, lines_to_the_end);
  editor_fix_cursor_position(editor);
}

static void editor_fix_cursor_position(Editor* editor) {
  const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  int line_length = buffer_row_get_length(current_row);

  if (line_length <= editor_get_cursor_x(editor)) {
    editor_home_cursor_x(editor);
    editor_move_cursor_x(editor, line_length, false);
  }
}

static void editor_move_cursor_x_to_right(Editor* editor, int x, bool insert_mode) {
  BufferRow* current_line = buffer_get_current_line(editor->current_buffer);
  const int line_length = buffer_row_get_length(current_line) - 1 + insert_mode;
  const int chars_till_end = line_length - editor_get_cursor_x(editor);
  const int chars_till_window_end = editor->window.width - editor->cursor.x - 1;
  if (x > chars_till_end) {
    x = chars_till_end;
  }

  if (x > chars_till_window_end) {
    editor->cursor.x = editor->window.width - 1;
    x -= chars_till_window_end;
  } else {
    editor->cursor.x += x;
    x = 0;
  }

  if (x > 0) {
    editor->start_column += x;
  }
}

static void editor_move_cursor_x_to_left(Editor* editor, int x) {
  const int chars_till_window = editor->cursor.x - editor->number_of_line_digits;

  if (x > chars_till_window) {
    x -= chars_till_window;
    editor->cursor.x = editor->number_of_line_digits;
  } else {
    editor->cursor.x -= x;
    x = 0;
  }

  if (x > editor->start_column) {
    editor->start_column = 0;
  } else {
    editor->start_column -= x;
  }
}

static void editor_move_cursor_x(Editor* editor, int x, bool insert_mode) {
  if (x >= 0) {
    editor_move_cursor_x_to_right(editor, x, insert_mode);
  } else {
    editor_move_cursor_x_to_left(editor, -x);
  }
}

static void editor_move_cursor_y(Editor* editor, int y) {
  const int previous_start = editor->start_line;
  editor->cursor.y += y;
  if (editor->cursor.y <= EDITOR_TOP_BAR_HEIGHT) {
    // Calculate how many lines past the top we went
    int lines_past_top = EDITOR_TOP_BAR_HEIGHT - editor->cursor.y + 1;
    editor->start_line -= lines_past_top;
    if (editor->start_line < 0) {
      editor->start_line = 0;
    }
    editor->cursor.y = EDITOR_TOP_BAR_HEIGHT;
  } else if (editor->cursor.y >= editor->window.height - EDITOR_BOTTOM_BAR_HEIGHT) {
    // Cursor went past the last content row
    const int number_of_lines = buffer_get_number_of_lines(editor->current_buffer);
    editor->start_line +=
      (editor->cursor.y - (editor->window.height - EDITOR_BOTTOM_BAR_HEIGHT - 1));
    editor->cursor.y = editor->window.height - EDITOR_BOTTOM_BAR_HEIGHT - 1;
    if (editor->start_line > number_of_lines - editor->window.height +
                               EDITOR_BOTTOM_BAR_HEIGHT + EDITOR_TOP_BAR_HEIGHT) {
      editor->start_line = number_of_lines - editor->window.height +
                           EDITOR_BOTTOM_BAR_HEIGHT + EDITOR_TOP_BAR_HEIGHT;
    }
  }
  if (editor->start_line != previous_start) {
    editor_mark_dirty_whole_screen(editor);
  }
}

static void editor_move_cursor_to_end(Editor* editor) {
  const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  editor_move_cursor_x(editor, buffer_row_get_length(current_row), false);
}

static void editor_process_editor_key(Editor* editor, int key) {
  // TODO: scroll buffers
  Buffer* current_buffer = editor->current_buffer;
  switch (key) {
    case 'h':
    case KEY_LEFT: {
      // Move cursor left
      editor->end_line_mode = false;
      editor_move_cursor_x(editor, -1, false);
      return;
    }
    case 'l':
    case KEY_RIGHT: {
      // Move cursor right
      editor_move_cursor_x(editor, 1, false);
      return;
    }
    case 'j':
    case KEY_DOWN: {
      // Move cursor down
      if (buffer_current_is_last_row(current_buffer)) {
        return;
      }
      editor_move_cursor_y(editor, 1);
      buffer_scroll_rows(editor->current_buffer, 1);
      editor_fix_cursor_position(editor);
      return;
    }
    case 'k':
    case KEY_UP: {
      // Move cursor up
      if (buffer_current_is_first_row(current_buffer)) {
        return;
      }
      editor_move_cursor_y(editor, -1);
      buffer_scroll_rows(editor->current_buffer, -1);
      editor_fix_cursor_position(editor);
      return;
    }
    case '^': {
      editor->end_line_mode = false;
      editor_move_cursor_to_start(editor);
      return;
    }
    case '$': {
      editor_move_cursor_to_end(editor);
      editor->end_line_mode = true;
      return;
    }
    case 'G': {
      editor->end_line_mode = false;
      editor_move_to_bottom(editor);
      return;
    }
    case 'w': {
      const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
      int offset_to_word =
        buffer_row_get_offset_to_next_word(current_row, editor_get_cursor_x(editor));

      editor_move_cursor_x(editor, offset_to_word, false);
      return;
    }
    case 'e': {
      const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
      int offset_to_end =
        buffer_row_get_offset_to_end_of_word(current_row, editor_get_cursor_x(editor));

      editor_move_cursor_x(editor, offset_to_end, false);
      return;
    }
    case 'b': {
      const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
      int offset_to_word =
        buffer_row_get_offset_to_prev_word(current_row, editor_get_cursor_x(editor));
      editor_move_cursor_x(editor, offset_to_word, false);
      return;
    }
    case '/': {
      // Start forward search
      editor_clear_error_message(editor);
      if (editor->search_buffer.buffer == NULL) {
        command_init(&editor->search_buffer);
      } else {
        editor->search_buffer.cursor_position = 0;
        editor->search_buffer.buffer[0] = '\0';
      }
      editor->state = EditorState_SearchInputForward;
      return;
    }
    case '?': {
      // Start backward search
      editor_clear_error_message(editor);
      if (editor->search_buffer.buffer == NULL) {
        command_init(&editor->search_buffer);
      } else {
        editor->search_buffer.cursor_position = 0;
        editor->search_buffer.buffer[0] = '\0';
      }
      editor->state = EditorState_SearchInputBackward;
      return;
    }
    case 'n': {
      // Find next match
      editor_search_next(editor);
      return;
    }
    case 'N': {
      // Find previous match
      editor_search_prev(editor);
      return;
    }
    case 'g': {
      editor->key_sequence[0] = 'g';
      return;
    }
    case 'd': {
      editor->key_sequence[0] = 'd';
      return;
    }
    case 'x': {
      BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
      if (buffer_row_remove_char(current_row, editor_get_cursor_x(editor))) {
        editor_fix_cursor_position(editor);
      }
      return;
    }
    case 'i': {
      editor->end_line_mode = false;
      editor->state = EditorState_EditMode;
      return;
    }
    case 'a': {
      editor->end_line_mode = false;
      editor->state = EditorState_EditMode;
      editor_move_cursor_x(editor, 1, true);
      return;
    }
    case 27: {
      editor_clear_error_message(editor);
      return;
    }
    default: {
      if (key >= '0' && key <= '9') {
        editor_process_key_sequence(editor, key);
      }
      return;
    }
  }
}

static bool editor_append_buffer(Editor* editor, Buffer* buffer) {
  Buffer** new_buffers = (Buffer**)realloc(
    editor->buffers, sizeof(Buffer*) * (editor->number_of_buffers + 1));
  if (new_buffers == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffers");
    return false;
  }
  editor->buffers = new_buffers;
  if (editor->buffers == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffers");
    return false;
  }

  editor->buffers[editor->number_of_buffers] = buffer;
  editor->number_of_buffers++;
  return true;
}

static int count_digits(int number) {
  int count = 0;
  if (number == 0) {
    return 1;
  }
  while (number > 0) {
    number /= 10;
    count++;
  }
  return count;
}

static attr_t editor_get_highlight_style(EHighlightToken token) {
  if (token < 0 || token >= EHighlightToken_Count) {
    token = EHighlightToken_Normal;
  }

  switch (token) {
    case EHighlightToken_Keyword:
      return COLOR_KEYWORD | A_BOLD;
    case EHighlightToken_String:
      return COLOR_STRING;
    case EHighlightToken_Comment:
      return COLOR_COMMENT;
    case EHighlightToken_Type:
      return COLOR_TYPE | A_BOLD;
    case EHighlightToken_Preprocessor:
      return COLOR_PREPROCESSOR;
    case EHighlightToken_Digit:
      return COLOR_NUMBER | A_BOLD;
    case EHighlightToken_Symbol:
      return COLOR_STRING | A_BOLD;
    case EHighlightToken_Keyword2:
      return COLOR_TYPE | A_BOLD;
    case EHighlightToken_Symbol2:
      return COLOR_NUMBER | A_BOLD;
    case EHighlightToken_Normal:
    case EHighlightToken_Count:
    default:
      return COLOR_OTHER;
  }
}

static void editor_decorate_and_draw_line(Editor* editor,
                                          int line_number,
                                          BufferRow* row,
                                          int line_number_width) {
  const char* line = &row->data[editor->start_column];
  const unsigned char* hl =
    (const unsigned char*)&row->highlight_data[editor->start_column];
  const int max_columns = editor->window.width - line_number_width;

  if (max_columns <= 0) {
    return;
  }

  move(line_number, line_number_width);
  attrset(COLOR_OTHER);

  EHighlightToken token = EHighlightToken_Normal;
  for (int i = 0; i < row->len - editor->start_column && i < max_columns; ++i) {
    if (line[i] == '\0') {
      break;
    }
    if (token != (EHighlightToken)hl[i]) {
      token = (EHighlightToken)hl[i];
      attrset(editor_get_highlight_style(token));
    }
    addch(line[i]);
  }

  attrset(COLOR_OTHER);
}

int editor_get_content_offset_x(const Editor* editor) {
  if (editor->file_manager && editor->file_manager->visible) {
    return editor->file_manager->width;
  }
  return 0;
}

static void editor_draw_buffers(Editor* editor) {
  int line_number = EDITOR_TOP_BAR_HEIGHT;
  editor->string_rendering_ongoing = false;
  editor->multiline_comment_ongoing = false;

  // Calculate content offset based on file manager visibility
  int content_offset = editor_get_content_offset_x(editor);
  editor->editor_offset_x = content_offset;

  if (editor->end_line_mode) {
    editor_move_cursor_to_end(editor);
  }
  if (editor->current_buffer != NULL) {
    // Draw current buffer
    // Content area: from EDITOR_TOP_BAR_HEIGHT to (window.height - EDITOR_BOTTOM_BAR_HEIGHT)
    const int window_height = editor->window.height - EDITOR_BOTTOM_BAR_HEIGHT;
    BufferRow* row = buffer_get_row(editor->current_buffer, editor->start_line);
    // Fixed 4-digit line number width to prevent content shifting
    // when line count goes from 99 to 100, 999 to 1000, etc.
    (void)count_digits;  // Unused now, but kept for potential future use
    editor->number_of_line_digits = 4 + content_offset;
    if (editor->cursor.x <= editor->number_of_line_digits) {
      editor->cursor.x = editor->number_of_line_digits;
    }

    while (line_number < window_height) {
      // Calculate actual row number: subtract top bar height and add start_line offset
      int row_number = line_number - EDITOR_TOP_BAR_HEIGHT + 1 + editor->start_line;

      if (row != NULL && row->dirty) {
        static char line_buffer[32];
        itoa(row_number, line_buffer, 10);
        int line_length = strlen(line_buffer);
        for (int i = line_length; i < editor->number_of_line_digits - content_offset; ++i) {
          line_buffer[i] = ' ';
        }
        line_buffer[editor->number_of_line_digits - content_offset] = '\0';

        attrset(COLOR_OTHER);
        mvaddstr(line_number, content_offset, line_buffer);
        if (editor->start_column < buffer_row_get_length(row)) {
          editor_decorate_and_draw_line(editor, line_number, row,
                                        editor->number_of_line_digits);
        }
        attrset(COLOR_OTHER);
        clrtoeol();
        row->dirty = false;
        row = row->next;
      } else if (row != NULL) {
        row = row->next;
      } else if (row == NULL) {
        move(line_number, content_offset);
        clrtoeol();
      }

      line_number++;
    }
  }
}

static void editor_draw_file_manager(Editor* editor) {
  if (!editor->file_manager || !editor->file_manager->visible) {
    return;
  }

  const int window_height =
    editor->window.height - EDITOR_BOTTOM_BAR_HEIGHT - EDITOR_TOP_BAR_HEIGHT + 1;

  file_manager_draw(editor->file_manager, 1, window_height);
}

static void editor_process_gkey_sequence(Editor* editor, int key) {
  if (key == 'g') {
    editor->key_sequence[0] = 0;
    editor_move_to_top(editor);
    editor_fix_cursor_position(editor);
  } else if (key == 't') {
    editor->key_sequence[0] = 0;
    // If repeat_count is set, switch to that buffer index directly
    // e.g., 2gt switches to buffer 2 (1-indexed for user)
    if (editor->repeat_count > 0) {
      size_t target_index = (size_t)editor->repeat_count;
      if (target_index < editor->number_of_buffers) {
        editor_switch_to_buffer_by_index(editor, target_index);
      } else {
        editor_set_error_message(editor, "Buffer index out of range");
      }
      editor->repeat_count = 0;
    } else {
      editor_switch_to_next_buffer(editor);
    }
  } else if (key == 'T') {
    editor->key_sequence[0] = 0;
    editor_switch_to_prev_buffer(editor);
  } else {
    editor->key_sequence[0] = 0;
  }
}

static void editor_process_dkey_sequence(Editor* editor, int key) {
  BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  const int number_of_lines = buffer_get_number_of_lines(editor->current_buffer);
  switch (key) {
    case 'd': {
      if (number_of_lines <= 1) {
        buffer_row_replace_line(current_row, "\n");
      } else {
        int offset = buffer_remove_current_row(editor->current_buffer);

        if (offset < 0) {
          editor_move_cursor_y(editor, -1);
        }
      }
      editor_mark_dirty_from_cursor(editor);
    } break;
    case 'w': {
      // Delete word
      int offset_to_word =
        buffer_row_get_offset_to_next_word(current_row, editor_get_cursor_x(editor));
      if (offset_to_word > 0) {
        buffer_row_remove_chars(current_row, editor_get_cursor_x(editor),
                                offset_to_word);
      }
    } break;
    case 'e': {
      // Delete to end of word
      int offset_to_end =
        buffer_row_get_offset_to_end_of_word(current_row, editor_get_cursor_x(editor));
      if (offset_to_end > 0) {
        buffer_row_remove_chars(current_row, editor_get_cursor_x(editor),
                                offset_to_end + 1);  // +1 to include current char
      }
    } break;
  }

  editor_fix_cursor_position(editor);
  editor->key_sequence[0] = 0;
}

static bool editor_process_key_sequence(Editor* editor, int key) {
  const size_t current_length = strlen(editor->key_sequence);
  if (current_length >= sizeof(editor->key_sequence) - 1) {
    editor->key_sequence[0] = '\0';
    return true;
  }

  if (editor->key_sequence[0] == 0 ||
      (editor->key_sequence[0] >= '0' && editor->key_sequence[0] <= '9')) {
    if (key >= '0' && key <= '9') {
      editor->key_sequence[current_length] = (char)key;
      editor->key_sequence[current_length + 1] = '\0';
      return true;
    } else {
      editor->repeat_count = atoi(editor->key_sequence) - 1;
      editor->key_sequence[0] = '\0';
      return false;
    }
  }
  switch (key) {
    case 27: {
      editor->key_sequence[0] = '\0';
      return true;
    }
    default: {
      if (editor->key_sequence[0] == 'g') {
        editor_process_gkey_sequence(editor, key);
        return true;
      } else if (editor->key_sequence[0] == 'd') {
        editor_process_dkey_sequence(editor, key);
        return true;
      }
      editor->key_sequence[0] = 0;
      return true;
    }
  }
  return true;
}

void editor_insert_char(Editor* editor, int key) {
  BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  switch (key) {
    case KEY_LEFT: {
      // Move cursor left
      editor->end_line_mode = false;
      editor_move_cursor_x(editor, -1, true);
      return;
    }
    case KEY_RIGHT: {
      // Move cursor right
      editor_move_cursor_x(editor, 1, true);
      return;
    }
    case KEY_UP: {
      // Move cursor up
      if (buffer_current_is_first_row(editor->current_buffer)) {
        return;
      }
      editor_move_cursor_y(editor, -1);
      buffer_scroll_rows(editor->current_buffer, -1);
      editor_fix_cursor_position(editor);
      return;
    }
    case KEY_DOWN: {
      // Move cursor down
      if (buffer_current_is_last_row(editor->current_buffer)) {
        return;
      }
      editor_move_cursor_y(editor, 1);
      buffer_scroll_rows(editor->current_buffer, 1);
      editor_fix_cursor_position(editor);
      return;
    }
    case KEY_BACKSPACE:
    case 127: {
      // Handle backspace
      if (editor->cursor.x > editor->number_of_line_digits) {
        editor_move_cursor_x(editor, -1, true);
        buffer_row_remove_char(current_row, editor_get_cursor_x(editor));
      } else if (editor->cursor.x == editor->number_of_line_digits) {
        int chars = buffer_join_current_line_with_previous(editor->current_buffer);
        if (chars > 0) {
          editor_move_cursor_y(editor, -1);
          editor_move_cursor_x(editor, editor->current_buffer->current_row->len,
                               false);
          editor_move_cursor_x(editor, -chars + 1, true);
        }
      }
      return;
    }
    case '\n': {
      editor_mark_dirty_from_cursor(editor);
      buffer_break_current_line(editor->current_buffer, editor_get_cursor_x(editor));
      editor_move_cursor_y(editor, 1);
      editor_home_cursor_x(editor);
      buffer_scroll_rows(editor->current_buffer, 1);
      return;
    }
    case '\t': {
      // Insert tab character
      for (int i = 0; i < editor->tab_size; ++i) {
        buffer_row_insert_char(current_row, editor_get_cursor_x(editor), ' ');
        editor_move_cursor_x(editor, 1, true);
      }
      return;
    }
    default: {
      buffer_row_insert_char(current_row, editor_get_cursor_x(editor), (char)key);
      editor_move_cursor_x(editor, 1, true);
    }
  };
}

static void editor_process_file_manager_key(Editor* editor, int key) {
  switch (key) {
    case 27:  // Escape
      editor->state = EditorState_Running;
      break;
    case 'j':
    case KEY_DOWN:
      file_manager_move_cursor_down(editor->file_manager);
      break;
    case 'k':
    case KEY_UP:
      file_manager_move_cursor_up(editor->file_manager);
      break;
    case 'h':
    case KEY_LEFT:
      file_manager_go_to_parent(editor->file_manager);
      break;
    case 'l':
    case KEY_RIGHT:
    case '\n':  // Enter
    case 13:
      editor_file_manager_select(editor);
      break;
    case ' ':
    case 'o':
      file_manager_toggle_expand(editor->file_manager);
      break;
    case 2:  // Ctrl+B
      editor_toggle_file_manager(editor);
      editor->state = EditorState_Running;
      break;
  }
}

void editor_process_key(Editor* editor, int key) {
  // Update debug keystroke widget if available
  if (editor->toolbar) {
    Widget* debug_widget = toolbar_find_widget(editor->toolbar, "debug_keystroke");
    if (debug_widget) {
      widget_debug_keystroke_add_key(debug_widget, key);
    }
  }

  // Handle Ctrl+B to toggle file manager from any state except editing
  if (key == 2 && editor->state != EditorState_EditMode) {
    if (editor->file_manager && editor->file_manager->visible) {
      // If file manager is visible, toggle it off and return to running
      editor_toggle_file_manager(editor);
      editor->state = EditorState_Running;
    } else {
      // Toggle on and switch to file manager state
      editor_toggle_file_manager(editor);
      editor->state = EditorState_FileManager;
    }
    return;
  }

  bool done = false;
  editor->key = key;
  while (!done) {
    switch (editor->state) {
      case EditorState_CollectingCommand: {
        if (!editor_collect_command(editor, key)) {
          return;
        }
      } break;
      case EditorState_ProcessingCommand: {
        int command_result = editor_process_command(editor);
        switch (command_result) {
          case CommandResult_ShouldExit: {
            editor->state = EditorState_Exiting;
          } break;
          case CommandResult_CommandNotFound: {
            editor_set_error_message(editor, "Command not found");
            editor->state = EditorState_Running;
          } break;
          case CommandResult_Success: {
            editor->state = EditorState_Running;
            editor->command.buffer[0] = '\0';  // Clear command buffer
          } break;
          default: {
          }
        }
        command_deinit(&editor->command);
        return;
      } break;
      case EditorState_Running:
        if (editor->key_sequence[0] != 0) {
          if (editor_process_key_sequence(editor, key)) {
            return;
          }
        }
        switch (key) {
          case ':': {
            if (editor->error_message) {
              editor_clear_error_message(editor);
            }
            command_init(&editor->command);
            editor->state = EditorState_CollectingCommand;
            return;
          } break;
          default: {
            editor_restore_cursor_position(editor);
            for (int i = 0; i < editor->repeat_count + 1; ++i) {
              editor_process_editor_key(editor, key);
              move(editor->cursor.y, editor->cursor.x);
            }
            editor->repeat_count = 0;
            return;
          }
        }
        break;
      case EditorState_EditMode:
        if (key == 27) {
          editor->state = EditorState_Running;
          // just in case we are after last character while appeding/removing last
          editor_fix_cursor_position(editor);
          return;
        }
        editor_insert_char(editor, key);
        return;
      case EditorState_FileManager:
        editor_process_file_manager_key(editor, key);
        return;
      case EditorState_SearchInputForward:
      case EditorState_SearchInputBackward: {
        if (!editor_collect_search_pattern(editor, key)) {
          return;
        }
      } break;
      case EditorState_Exiting:
        return;
    };
  }
}

bool editor_should_exit(const Editor* editor) {
  return editor->state == EditorState_Exiting;
}

void editor_draw_status_bar(const Editor* editor) {
  if (editor->state == EditorState_CollectingCommand) {
    if (editor->command.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, ':');
      mvaddstr(editor->window.height - 1, 1, editor->command.buffer);
    }
  }
  if (editor->state == EditorState_SearchInputForward) {
    if (editor->search_buffer.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, '/');
      mvaddstr(editor->window.height - 1, 1, editor->search_buffer.buffer);
    } else {
      mvaddch(editor->window.height - 1, 0, '/');
    }
  }
  if (editor->state == EditorState_SearchInputBackward) {
    if (editor->search_buffer.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, '?');
      mvaddstr(editor->window.height - 1, 1, editor->search_buffer.buffer);
    } else {
      mvaddch(editor->window.height - 1, 0, '?');
    }
  }
  if (editor->error_message) {
    mvaddstr(editor->window.height - 1, 1, editor->error_message);
  }
  if (editor->status_bar) {
    mvaddstr(editor->window.height - 2, 0, editor->status_bar);
  }
  if (editor->key_sequence[0] != 0) {
    mvaddstr(editor->window.height - 1, editor->window.width - 10,
             editor->key_sequence);
  }
  mvprintw(editor->window.height - 1, editor->window.width - 30, "'%c'(%d) ",
           editor->key, editor->key);
}

void editor_redraw_screen(Editor* editor) {
  // clear();
  curs_set(0);
  editor_draw_tab_bar(editor);
  editor_draw_file_manager(editor);
  editor_draw_buffers(editor);

  // Use widget-based toolbar if available
  if (editor->toolbar) {
    toolbar_update(editor->toolbar, editor);
    toolbar_draw(editor->toolbar, editor);
  } else {
    // Fallback to old status bar
    editor_draw_status_bar(editor);
  }

  // Draw search input line if in search mode (overrides toolbar/status bar)
  if (editor->state == EditorState_SearchInputForward) {
    if (editor->search_buffer.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, '/');
      mvaddstr(editor->window.height - 1, 1, editor->search_buffer.buffer);
    } else {
      mvaddch(editor->window.height - 1, 0, '/');
    }
  } else if (editor->state == EditorState_SearchInputBackward) {
    if (editor->search_buffer.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, '?');
      mvaddstr(editor->window.height - 1, 1, editor->search_buffer.buffer);
    } else {
      mvaddch(editor->window.height - 1, 0, '?');
    }
  }

  switch (editor->state) {
    case EditorState_Running:
    case EditorState_EditMode:
      editor_restore_cursor_position(editor);
      break;
    default:
      break;
  }
  window_redraw_screen(&editor->window);
  curs_set(1);
}

void editor_init(Editor* editor) {
  window_init(&editor->window);

  // Create and initialize file manager
  editor->file_manager = file_manager_alloc();
  if (editor->file_manager) {
    file_manager_init(editor->file_manager, NULL);
    editor->file_manager->visible = false;
  }
  editor->editor_offset_x = 0;

  // Initialize search state
  search_init(&editor->search);
  command_init(&editor->search_buffer);

  // Create and initialize toolbar
  editor->toolbar = malloc(sizeof(struct Toolbar));
  if (editor->toolbar) {
    toolbar_init(editor->toolbar, editor->window.height, editor->window.width);

    // Add default widgets
    toolbar_add_widget(editor->toolbar, widget_mode_create());
    toolbar_add_widget(editor->toolbar, widget_filename_create());
    toolbar_add_widget(editor->toolbar, widget_position_create());
    toolbar_add_widget(editor->toolbar, widget_command_create());
    toolbar_add_widget(editor->toolbar, widget_message_create());
    toolbar_add_widget(editor->toolbar, widget_keyseq_create());

#ifdef DEBUG
    // Add debug keystroke widget in debug builds
    DebugKeystrokeConfig debug_config = {
        .show_ascii_char = true,
        .show_decimal_code = true,
        .show_hex_code = true,
        .show_octal_code = false,
        .compact_mode = true,
    };
    toolbar_add_widget(editor->toolbar, widget_debug_keystroke_create(&debug_config));
#endif

    toolbar_layout(editor->toolbar);
  }

  editor_home_cursor_xy(editor);
  editor->current_buffer_index = 0;
  move(editor->cursor.y, editor->cursor.x);
}

void editor_deinit(Editor* editor) {
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    buffer_free(editor->buffers[i]);
  }
  free(editor->buffers);
  editor->buffers = NULL;
  editor->number_of_buffers = 0;
  editor->current_buffer = NULL;
  if (editor->error_message) {
    free(editor->error_message);
    editor->error_message = NULL;
  }
  // Clean up command buffer if it was initialized
  if (editor->command.buffer != NULL) {
    command_deinit(&editor->command);
  }
  // Clean up toolbar
  if (editor->toolbar) {
    toolbar_deinit(editor->toolbar);
    free(editor->toolbar);
    editor->toolbar = NULL;
  }
  // Clean up file manager
  if (editor->file_manager) {
    file_manager_free(editor->file_manager);
    editor->file_manager = NULL;
  }
  // Clean up search state
  search_deinit(&editor->search);
  if (editor->search_buffer.buffer != NULL) {
    command_deinit(&editor->search_buffer);
  }
  window_deinit(&editor->window);
}

void editor_toggle_file_manager(Editor* editor) {
  if (!editor->file_manager) {
    return;
  }

  // Get current offset before toggle
  int old_offset = editor_get_content_offset_x(editor);

  file_manager_toggle_visibility(editor->file_manager);

  // Get new offset after toggle
  int new_offset = editor_get_content_offset_x(editor);
  int offset_delta = new_offset - old_offset;
  
  // Update editor_offset_x immediately (also updated during draw)
  editor->editor_offset_x = new_offset;

  // Mark all rows dirty to force redraw
  editor_mark_dirty_whole_screen(editor);

  // Adjust cursor position by the offset change
  editor->number_of_line_digits = 4 + new_offset;
  editor->cursor.x += offset_delta;

  // Ensure cursor stays within valid bounds
  if (editor->cursor.x < editor->number_of_line_digits) {
    editor->cursor.x = editor->number_of_line_digits;
  }
}

static Buffer* editor_find_buffer_by_filename(Editor* editor, const char* filename) {
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    const char* buf_filename = buffer_get_filename(editor->buffers[i]);
    if (buf_filename && strcmp(buf_filename, filename) == 0) {
      return editor->buffers[i];
    }
  }
  return NULL;
}

void editor_file_manager_select(Editor* editor) {
  if (!editor->file_manager || !editor->file_manager->cursor) {
    return;
  }

  FileManagerEntry* entry = file_manager_get_selected_entry(editor->file_manager);
  if (!entry) {
    return;
  }

  if (entry->type == FileManagerEntryType_Directory) {
    file_manager_toggle_expand(editor->file_manager);
  } else {
    // Check if file is already open in a buffer
    Buffer* existing = editor_find_buffer_by_filename(editor, entry->full_path);
    if (existing) {
      // Find the index of the existing buffer
      for (size_t i = 0; i < editor->number_of_buffers; ++i) {
        if (editor->buffers[i] == existing) {
          editor->current_buffer_index = i;
          break;
        }
      }
      editor->current_buffer = existing;
      buffer_scroll_to_top(editor->current_buffer);
      editor_set_error_message(editor, "Switched to buffer");
    } else {
      // Load the file into a new buffer
      Buffer* buffer = buffer_alloc();
      if (buffer == NULL) {
        editor_set_error_message(editor, "Failed to allocate memory for buffer");
        return;
      }
      buffer_load_from_file(buffer, entry->full_path);
      if (!editor_append_buffer(editor, buffer)) {
        editor_set_error_message(editor, "Failed to append buffer");
        buffer_free(buffer);
        return;
      }
      editor->current_buffer = buffer;
      editor->current_buffer_index = editor->number_of_buffers - 1;
      editor_set_error_message(editor, "Loaded file");
    }

    // Reset view to top of file
    editor->start_line = 0;
    editor->start_column = 0;
    editor_mark_dirty_whole_screen(editor);

    // Close file manager and switch back to normal mode
    file_manager_set_visible(editor->file_manager, false);
    editor->state = EditorState_Running;

    // Adjust cursor position after closing file manager (shift left by sidebar width)
    editor->number_of_line_digits = 4;  // Reset to base value
    editor->cursor.x = editor->number_of_line_digits;
    editor->cursor.y = EDITOR_TOP_BAR_HEIGHT;  // First line of content
  }
}

void editor_load_file(Editor* editor, const char* filename) {
  Buffer* buffer = buffer_alloc();
  if (buffer == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffer");
    return;
  }
  buffer_load_from_file(buffer, filename);
  if (!editor_append_buffer(editor, buffer)) {
    editor_set_error_message(editor, "Failed to append buffer");
    buffer_free(buffer);
    return;
  }
  if (editor->current_buffer == NULL) {
    editor->current_buffer = buffer;
    editor->current_buffer_index = 0;
  }
}

void editor_create_new_file(Editor* editor) {
  Buffer* buffer = buffer_alloc();
  if (buffer == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffer");
    return;
  }
  buffer_append_line(buffer, "\n");  // Start with an empty line
  if (!editor_append_buffer(editor, buffer)) {
    editor_set_error_message(editor, "Failed to append buffer");
    buffer_free(buffer);
    return;
  }
  if (editor->current_buffer == NULL) {
    editor->current_buffer = buffer;
    editor->current_buffer_index = 0;
  }
}

// ============================================================================
// Buffer/Tab Management
// ============================================================================

void editor_switch_to_buffer_by_index(Editor* editor, size_t index) {
  if (index >= editor->number_of_buffers) {
    editor_set_error_message(editor, "Invalid buffer index");
    return;
  }
  editor->current_buffer_index = index;
  editor->current_buffer = editor->buffers[index];
  buffer_scroll_to_top(editor->current_buffer);
  editor->start_line = 0;
  editor->start_column = 0;
  editor_home_cursor_xy(editor);
  editor_mark_dirty_whole_screen(editor);
}

void editor_switch_to_next_buffer(Editor* editor) {
  if (editor->number_of_buffers <= 1) {
    return;
  }
  size_t next_index = editor->current_buffer_index + 1;
  if (next_index >= editor->number_of_buffers) {
    next_index = 0;  // Wrap around to first buffer
  }
  editor_switch_to_buffer_by_index(editor, next_index);
}

void editor_switch_to_prev_buffer(Editor* editor) {
  if (editor->number_of_buffers <= 1) {
    return;
  }
  size_t prev_index;
  if (editor->current_buffer_index == 0) {
    prev_index = editor->number_of_buffers - 1;  // Wrap around to last buffer
  } else {
    prev_index = editor->current_buffer_index - 1;
  }
  editor_switch_to_buffer_by_index(editor, prev_index);
}

void editor_close_current_buffer(Editor* editor) {
  if (editor->number_of_buffers <= 1) {
    editor_set_error_message(editor, "Cannot close the last buffer");
    return;
  }

  // Free the current buffer
  buffer_free(editor->buffers[editor->current_buffer_index]);

  // Shift remaining buffers down
  for (size_t i = editor->current_buffer_index; i < editor->number_of_buffers - 1; i++) {
    editor->buffers[i] = editor->buffers[i + 1];
  }
  editor->number_of_buffers--;

  // Reallocate the buffers array
  Buffer** new_buffers = (Buffer**)realloc(
    editor->buffers, sizeof(Buffer*) * editor->number_of_buffers);
  if (new_buffers != NULL || editor->number_of_buffers == 0) {
    editor->buffers = new_buffers;
  }

  // Switch to the previous buffer, or keep current index if possible
  if (editor->current_buffer_index > 0) {
    editor->current_buffer_index--;
  }
  // Otherwise stay at 0 (the new first buffer after shifting)
  editor->current_buffer = editor->buffers[editor->current_buffer_index];
  buffer_scroll_to_top(editor->current_buffer);
  editor->start_line = 0;
  editor->start_column = 0;
  editor_home_cursor_xy(editor);
  editor_mark_dirty_whole_screen(editor);
}

void editor_draw_tab_bar(Editor* editor) {
  if (editor->number_of_buffers == 0) {
    return;
  }

  // Clear the tab bar area (row 0)
  attrset(COLOR_OTHER);
  move(0, 0);
  for (int i = 0; i < editor->window.width; i++) {
    addch(' ');
  }

  int x = 0;
  const int max_tab_width = 20;
  const int min_tab_width = 8;

  for (size_t i = 0; i < editor->number_of_buffers; i++) {
    if (x >= editor->window.width - min_tab_width) {
      // Not enough space for more tabs, show indicator
      attrset(COLOR_OTHER | A_BOLD);
      mvaddstr(0, editor->window.width - 3, "...");
      attrset(COLOR_OTHER);
      break;
    }

    const char* filename = buffer_get_filename(editor->buffers[i]);
    if (!filename) {
      filename = "[No Name]";
    }

    // Extract just the basename from the path
    const char* basename = filename;
    const char* last_slash = strrchr(filename, '/');
    if (last_slash) {
      basename = last_slash + 1;
    }

    // Calculate tab width
    int name_len = strlen(basename);
    bool is_modified = buffer_is_modified(editor->buffers[i]);
    int tab_width = name_len + (is_modified ? 2 : 0) + 3;  // name + [+] + brackets + space
    if (tab_width > max_tab_width) {
      tab_width = max_tab_width;
    }
    if (tab_width < min_tab_width) {
      tab_width = min_tab_width;
    }

    // Ensure we don't overflow
    if (x + tab_width > editor->window.width) {
      tab_width = editor->window.width - x;
      if (tab_width < 4) break;
    }

    // Draw tab with highlighting for current buffer
    if (i == editor->current_buffer_index) {
      attrset(COLOR_PAIR(4) | A_BOLD | A_REVERSE);  // Yellow background for active tab
    } else {
      attrset(COLOR_OTHER | A_DIM);
    }

    // Draw tab content
    char tab_content[32];
    int content_len = snprintf(tab_content, sizeof(tab_content), " %s%s ",
                               basename, is_modified ? "+" : "");
    if (content_len > tab_width - 1) {
      // Truncate and add ellipsis
      tab_content[tab_width - 2] = '\0';
      strncat(tab_content, " ", sizeof(tab_content) - strlen(tab_content) - 1);
    }

    mvaddnstr(0, x, tab_content, tab_width - 1);
    x += tab_width;

    // Draw separator between tabs
    if (i < editor->number_of_buffers - 1 && x < editor->window.width) {
      attrset(COLOR_OTHER | A_DIM);
      mvaddch(0, x - 1, '|');
    }
  }

  attrset(COLOR_OTHER);

  // Draw separator line (row 1)
  attrset(COLOR_OTHER | A_DIM);
  move(1, 0);
  for (int i = 0; i < editor->window.width; i++) {
    addch('-');
  }
  attrset(COLOR_OTHER);
}
