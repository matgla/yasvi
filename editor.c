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

#define EDITOR_TOP_BAR_HEIGHT 1
// 1 is for command line
// 2 is for status bar
#define EDITOR_BOTTOM_BAR_HEIGHT 2

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

static int editor_get_cursor_x(const Editor* editor) {
  if (editor->cursor.x + editor->start_column < editor->number_of_line_digits) {
    return 0;
  }
  return editor->cursor.x + editor->start_column - editor->number_of_line_digits;
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
  editor->cursor.y = 1;
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
    editor->start_line += (editor->cursor.y - 1);
    if (editor->start_line < 0) {
      editor->start_line = 0;
    }
    editor->cursor.y = 1;
  } else if (editor->cursor.y > editor->window.height - EDITOR_BOTTOM_BAR_HEIGHT -
                                  EDITOR_TOP_BAR_HEIGHT) {
    const int number_of_lines = buffer_get_number_of_lines(editor->current_buffer);
    editor->start_line +=
      (editor->cursor.y -
       (editor->window.height - EDITOR_BOTTOM_BAR_HEIGHT - EDITOR_TOP_BAR_HEIGHT));
    editor->cursor.y =
      editor->window.height - EDITOR_BOTTOM_BAR_HEIGHT - EDITOR_TOP_BAR_HEIGHT;
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
    case 'b': {
      const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
      int offset_to_word =
        buffer_row_get_offset_to_prev_word(current_row, editor_get_cursor_x(editor));
      editor_move_cursor_x(editor, offset_to_word, false);
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
  editor->buffers = NULL;
  editor->buffers = (Buffer**)realloc(
    editor->buffers, sizeof(Buffer*) * (editor->number_of_buffers + 1));
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

static const char* highlight_styles[] = {
  "\e[0;39;49m", "\e[1;34;40m", "\e[0;32;40m", "\e[2;37;40m",
  "\e[1;35;40m", "\e[1;31;40m", "\e[0;35;40m", "\e[0;33;40m",
};
static const char* highlight_additional_style[] = {
  NULL, NULL, NULL, "\e[3m", NULL, NULL, NULL, NULL,
};

static int editor_write_highlight_style(Editor* editor,
                                        EHighlightToken token,
                                        char* buffer,
                                        int n) {
  if (n <= 15) {
    return 0;
  }

  if (token < 0 || token >= EHighlightToken_Count) {
    token = EHighlightToken_Normal;
  }
  const char* style = highlight_styles[token];
  memcpy(buffer, style, 10);
  if (highlight_additional_style[token] != NULL) {
    memcpy(buffer + 10, highlight_additional_style[token], 4);
    buffer[15] = '\0';
    return 14;
  }
  buffer[10] = '\0';
  return 10;
}

static void editor_decorate_and_draw_line(Editor* editor,
                                          int line_number,
                                          BufferRow* row,
                                          char* buffer,
                                          int n) {
  const char* line = &row->data[editor->start_column];
  const char* hl = &row->highlight_data[editor->start_column];
  int index = 0;
  EHighlightToken token = EHighlightToken_Normal;
  for (int i = 0; i < row->len - editor->start_column && index < n; ++i) {
    if (line[i] == '\0') {
      break;  // End of line
    }
    if (token != hl[i]) {
      token = hl[i];
      index +=
        editor_write_highlight_style(editor, token, &buffer[index], n - index);
    }
    if (index >= n - 1) {
      break;  // No more space in the buffer
    }
    buffer[index++] = line[i];
  }
  buffer[index] = '\0';
}

static void editor_draw_buffers(Editor* editor) {
  int line_number = 1;
  editor->string_rendering_ongoing = false;
  editor->multiline_comment_ongoing = false;

  if (editor->end_line_mode) {
    editor_move_cursor_to_end(editor);
  }
  if (editor->current_buffer != NULL) {
    // Draw current buffer
    const int window_height =
      editor->window.height - EDITOR_BOTTOM_BAR_HEIGHT - EDITOR_TOP_BAR_HEIGHT + 1;
    BufferRow* row = buffer_get_row(editor->current_buffer, editor->start_line);
    int max_digits = count_digits(editor->start_line + window_height);

    editor->number_of_line_digits = max_digits + 1;
    if (editor->cursor.x <= editor->number_of_line_digits) {
      editor->cursor.x = editor->number_of_line_digits;
    }

    while (line_number < window_height) {
      int row_number = line_number + editor->start_line;

      if (row != NULL && row->dirty) {
        static char line_buffer[1024];
        memcpy(line_buffer, highlight_styles[EHighlightToken_Normal], 10);
        itoa(row_number, line_buffer + 10, 10);
        int line_length = strlen(line_buffer);
        int i = 0;
        for (i = 0; i < editor->number_of_line_digits - line_length + 11; ++i) {
          line_buffer[line_length] = ' ';
          ++line_length;
        }
        line_buffer[line_length] = '\0';
        if (editor->start_column < buffer_row_get_length(row)) {
          editor_decorate_and_draw_line(editor, line_number, row,
                                        &line_buffer[line_length],
                                        sizeof(line_buffer) - line_length);
        }
        mvaddstr(line_number, 0, line_buffer);
        clrtoeol();
        row->dirty = false;
        row = row->next;
      } else if (row != NULL) {
        row = row->next;
      } else if (row == NULL) {
        move(line_number, 0);
        clrtoeol();
      }

      line_number++;
    }
  }
}

static void editor_process_gkey_sequence(Editor* editor, int key) {
  if (key == 'g') {
    editor->key_sequence[0] = 0;
    editor_move_to_top(editor);
    editor_fix_cursor_position(editor);
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

void editor_process_key(Editor* editor, int key) {
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
  editor_draw_buffers(editor);
  editor_draw_status_bar(editor);
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
  editor_home_cursor_xy(editor);
  move(editor->cursor.y, editor->cursor.x);
}

void editor_deinit(Editor* editor) {
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    buffer_free(editor->buffers[i]);
  }
  free(editor->buffers);
  if (editor->error_message) {
    free(editor->error_message);
    editor->error_message = NULL;
  }
  window_deinit(&editor->window);
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
  }
}
