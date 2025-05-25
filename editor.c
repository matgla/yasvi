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

#include <string.h>

#include <ncurses.h>

#include "command.h"

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

static void editor_set_bar_message(Editor* editor, const char* message) {
  if (editor->status_bar) {
    free(editor->status_bar);
  }
  editor->status_bar = strdup(message);
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
    for (int i = 0; i < strlen(editor->error_message); ++i) {
      mvaddch(editor->window.height - 1, 1 + i, ' ');
    }
    free(editor->error_message);
    editor->error_message = NULL;
  }
}

static CommandResult editor_process_command(Editor* editor) {
  const Command* command = &editor->command;
  if (command->buffer == NULL) {
    return CommandResult_CommandNotFound;
  }
  if (strcmp(command->buffer, "q") == 0) {
    return CommandResult_ShouldExit;
  }

  if (strcmp(command->buffer, "w") == 0) {
    FILE* file = fopen(editor->current_buffer->filename, "w");
    if (file == NULL) {
      editor_set_error_message(editor, "Failed to open file for writing");
      return CommandResult_CommandNotFound;
    }
    for (BufferRow* row = editor->current_buffer->head; row != NULL;
         row = row->next) {
      if (row->data) {
        fprintf(file, "%s\n", row->data);
      }
    }

    fclose(file);
    editor_set_error_message(editor, "File saved successfully");
    command->buffer[0] = '\0';  // Clear command buffer
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
  if (editor->current_buffer == NULL) {
    return;
  }
  editor->cursor.x = editor->number_of_line_digits;
  if (editor->current_buffer) {
    editor->cursor.x += buffer_row_get_offset_to_first_char(
      editor->current_buffer->current_row, editor->current_buffer->start_column);
  }
  editor->current_buffer->start_column = 0;
}

static void editor_move_to_top(Editor* editor) {
  editor->current_buffer->current_row = editor->current_buffer->head;
  editor->cursor.y = 0;
  editor->current_buffer->start_line = 0;
}

static void editor_move_to_bottom(Editor* editor) {
  Buffer* buffer = editor->current_buffer;
  if (buffer) {
    int start_line = 0;
    if (buffer->number_of_rows > editor->window.height - 2) {
      start_line = buffer->number_of_rows - editor->window.height + 2;
      editor->cursor.y = editor->window.height - 3;
    } else {
      editor->cursor.y = buffer->number_of_rows - 1;
    }
    buffer->start_line = start_line;
  }
}

static void editor_fix_cursor_position_for_line(Editor* editor) {
  if (editor->current_buffer == NULL) {
    return;
  }
  int line_length = buffer_row_get_length(editor->current_buffer->current_row);

  if (line_length + editor->number_of_line_digits < editor->cursor.x) {
    editor->cursor.x = editor->number_of_line_digits;
    editor->current_buffer->start_column = 0;
    editor_move_cursor_x(editor, line_length, false);
  }
}

static void editor_move_to_next_row(Editor* editor) {
  if (editor->current_buffer->current_row->next) {
    editor->current_buffer->current_row = editor->current_buffer->current_row->next;
  }
}

static void editor_move_to_previous_row(Editor* editor) {
  if (editor->current_buffer->current_row->prev) {
    editor->current_buffer->current_row = editor->current_buffer->current_row->prev;
  }
}

static void editor_move_cursor_x_to_right(Editor* editor, int x, bool insert_mode) {
  BufferRow* current_line = editor->current_buffer->current_row;
  const int line_length = buffer_row_get_length(current_line) + insert_mode;
  const int chars_till_end = line_length - editor->current_buffer->start_column -
                             editor->cursor.x + editor->number_of_line_digits;
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
    editor->current_buffer->start_column += x;
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

  if (x > editor->current_buffer->start_column) {
    editor->current_buffer->start_column = 0;
  } else {
    editor->current_buffer->start_column -= x;
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
  editor->cursor.y += y;
  if (editor->cursor.y < 0) {
    editor->current_buffer->start_line += editor->cursor.y;
    if (editor->current_buffer->start_line < 0) {
      editor->current_buffer->start_line = 0;
    }
    editor->cursor.y = 0;
  } else if (editor->cursor.y >= editor->window.height - 2) {
    editor->current_buffer->start_line +=
      (editor->cursor.y - (editor->window.height + 3));
    editor->cursor.y = editor->window.height - 3;
    if (editor->current_buffer->start_line >
        editor->current_buffer->number_of_rows) {
      editor->current_buffer->start_line = editor->current_buffer->number_of_rows;
    }
  }
}

static void editor_move_cursor_to_end(Editor* editor) {
  editor_move_cursor_x(
    editor, buffer_row_get_length(editor->current_buffer->current_row), false);
}

static void editor_process_editor_key(Editor* editor, int key) {
  // TODO: scroll buffers
  Buffer* current_buffer = editor->current_buffer;
  switch (key) {
    case 'h': {
      // Move cursor left
      editor->end_line_mode = false;
      editor_move_cursor_x(editor, -1, false);
      return;
    }
    case 'l': {
      // Move cursor right
      editor_move_cursor_x(editor, 1, false);
      return;
    }
    case 'j': {
      // Move cursor down
      if (editor->cursor.y < editor->window.height - 3) {
        if (editor->cursor.y >= current_buffer->number_of_rows - 1) {
          return;
        }
        editor->cursor.y++;
        editor_move_to_next_row(editor);
      } else {
        // scroll buffer down
        if (editor->current_buffer->tail) {
          if (editor->current_buffer->start_line <
              editor->current_buffer->number_of_rows - editor->window.height + 2) {
            editor->current_buffer->start_line++;
            editor_move_to_next_row(editor);
          }
        }
      }
      editor_fix_cursor_position_for_line(editor);
      return;
    }
    case 'k': {
      // Move cursor up
      if (editor->cursor.y > 0) {
        editor->cursor.y--;
        editor_move_to_previous_row(editor);
      } else {
        if (editor->current_buffer && editor->current_buffer->start_line > 0) {
          editor->current_buffer->start_line--;
          editor_move_to_previous_row(editor);
        }
      }
      editor_fix_cursor_position_for_line(editor);
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
      editor->current_buffer->current_row = editor->current_buffer->tail;
      editor_move_to_bottom(editor);
      if (editor->cursor.x >= editor->current_buffer->current_row->len) {
        editor->cursor.x = editor->current_buffer->current_row->len;
      }
      return;
    }
    case 'w': {
      int offset_to_word = buffer_row_get_offset_to_next_word(
        editor->current_buffer->current_row, editor->cursor.x +
                                               editor->current_buffer->start_column -
                                               editor->number_of_line_digits);
      editor_move_cursor_x(editor, offset_to_word, false);
      return;
    }
    case 'b': {
      int offset_to_word = buffer_row_get_offset_to_prev_word(
        editor->current_buffer->current_row, editor->cursor.x +
                                               editor->current_buffer->start_column -
                                               editor->number_of_line_digits);
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
      if (editor->cursor.x + editor->current_buffer->start_column -
            editor->number_of_line_digits <=
          buffer_row_get_length(editor->current_buffer->current_row)) {
        buffer_row_remove_char(editor->current_buffer->current_row,
                               editor->cursor.x +
                                 editor->current_buffer->start_column -
                                 editor->number_of_line_digits);
      } else {
        buffer_row_remove_char(editor->current_buffer->current_row,
                               editor->current_buffer->current_row->len - 1);
        editor_move_cursor_x(editor, -1, false);
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

static void editor_draw_buffers(Editor* editor) {
  int line_number = 0;
  if (editor->end_line_mode) {
    editor_move_cursor_to_end(editor);
  }
  if (editor->current_buffer != NULL) {
    // Draw current buffer
    BufferRow* row =
      buffer_get_row(editor->current_buffer, editor->current_buffer->start_line);
    int max_digits =
      count_digits(editor->current_buffer->start_line + editor->window.height - 2);

    editor->number_of_line_digits = max_digits + 1;
    if (editor->cursor.x <= editor->number_of_line_digits) {
      editor->cursor.x = editor->number_of_line_digits;
    }

    while (row != NULL && line_number < editor->window.height - 2) {
      int row_number = line_number + editor->current_buffer->start_line + 1;
      mvprintw(line_number, 0, "%d ", row_number);

      int max_line_length = editor->window.width - editor->number_of_line_digits;
      if (editor->current_buffer->start_column < row->len) {
        mvaddnstr(line_number, editor->number_of_line_digits,
                  &row->data[editor->current_buffer->start_column], max_line_length);
      }
      line_number++;
      row = row->next;
    }
  }
}

static void editor_process_gkey_sequence(Editor* editor, int key) {
  if (key == 'g') {
    editor->key_sequence[0] = 0;
    editor_move_to_top(editor);
    editor_fix_cursor_position_for_line(editor);
  } else {
    editor->key_sequence[0] = 0;
  }
}

static void editor_process_dkey_sequence(Editor* editor, int key) {
  if (key == 'd') {
    if (editor->current_buffer->number_of_rows <= 1) {
      buffer_row_trim(editor->current_buffer->current_row, 0);
    } else if (editor->current_buffer->current_row) {
      BufferRow* next_row = editor->current_buffer->current_row->next;
      if (next_row == NULL) {
        next_row = editor->current_buffer->current_row->prev;
        editor_move_cursor_y(editor, -1);
      }
      buffer_remove_row(editor->current_buffer, editor->current_buffer->current_row);
      editor->current_buffer->current_row = next_row;
      editor->cursor.x = 0;
      editor->current_buffer->start_column = 0;
    }
  }
  editor->key_sequence[0] = 0;
}

static bool editor_process_key_sequence(Editor* editor, int key) {
  const int current_length = strlen(editor->key_sequence);
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
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", key);
  editor_set_bar_message(editor, buf);
  if (key < 0 || key > 255) {
    return;  // Invalid character
  }
  if (key == KEY_BACKSPACE || key == 127) {
    // Handle backspace
    if (editor->cursor.x > editor->number_of_line_digits) {
      editor_move_cursor_x(editor, -1, true);
      buffer_row_remove_char(editor->current_buffer->current_row,
                             editor->current_buffer->start_column +
                               editor->cursor.x - editor->number_of_line_digits);
    }
    return;
  }
  if (key == '\n') {
    if (editor->cursor.x - editor->number_of_line_digits >=
        editor->current_buffer->current_row->len) {
      buffer_insert_row_at(editor->current_buffer,
                           editor->current_buffer->current_row);
      editor->current_buffer->current_row =
        editor->current_buffer->current_row->next;
      editor_move_cursor_y(editor, 1);
    } else if (editor->cursor.x == editor->number_of_line_digits) {
      buffer_insert_row_at(editor->current_buffer,
                           editor->current_buffer->current_row->prev);
      editor->current_buffer->current_row =
        editor->current_buffer->current_row->prev;
    } else {
      // break the line at the cursor position
      buffer_insert_row_at(editor->current_buffer,
                           editor->current_buffer->current_row);
      editor->current_buffer->current_row =
        editor->current_buffer->current_row->next;

      buffer_row_insert_str(editor->current_buffer->current_row, 0,
                            editor->current_buffer->current_row->prev->data +
                              editor->current_buffer->start_column +
                              editor->cursor.x - editor->number_of_line_digits);
      buffer_row_trim(editor->current_buffer->current_row->prev,
                      editor->current_buffer->start_column + editor->cursor.x -
                        editor->number_of_line_digits);
      editor->cursor.x = 0;
      editor_move_cursor_y(editor, 1);
      return;
    }
    editor->current_buffer->start_column = 0;
    editor->cursor.x = editor->number_of_line_digits;

    return;
  }
  buffer_row_insert_char(editor->current_buffer->current_row,
                         editor->current_buffer->start_column + editor->cursor.x -
                           editor->number_of_line_digits,
                         (char)key);
  editor_move_cursor_x(editor, 1, true);
}

void editor_process_key(Editor* editor, int key) {
  bool done = false;
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
}

void editor_redraw_screen(Editor* editor) {
  clear();
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
}

void editor_init(Editor* editor) {
  window_init(&editor->window);
  editor->cursor.x = editor->number_of_line_digits;
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