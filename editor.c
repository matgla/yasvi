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
static void editor_move_cursor_x(Editor* editor, int x);

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

static CommandResult editor_process_command(const Command* command) {
  if (strcmp(command->buffer, "q") == 0) {
    return CommandResult_ShouldExit;
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

static void editor_move_to_bottom(Editor* editor) {
  Buffer* buffer = editor->current_buffer;
  if (buffer) {
    int start_line = 0;
    if (buffer->number_of_rows > editor->window.height - 2) {
      start_line = buffer->number_of_rows - editor->window.height + 2;
      editor->cursor.y = editor->window.height - 3;
    } else {
      editor->cursor.y = buffer->number_of_rows;
    }
    buffer->start_line = start_line;
  }
}

static void editor_fix_cursor_position_for_line(Editor* editor) {
  if (editor->current_buffer == NULL) {
    return;
  }
  int line_length = buffer_row_get_length(editor->current_buffer->current_row);

  if (line_length < editor->cursor.x) {
    editor->cursor.x = editor->number_of_line_digits;
    editor->current_buffer->start_column = 0;
    editor_move_cursor_x(editor, line_length);
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

static void editor_move_cursor_x(Editor* editor, int x) {
  BufferRow* current_line = editor->current_buffer->current_row;
  const int line_length = buffer_row_get_length(current_line);
  const int chars_till_end = line_length - editor->current_buffer->start_column -
                             editor->cursor.x + editor->number_of_line_digits;
  const int chars_till_window_end = editor->window.width - editor->cursor.x - 1;
  if (x > chars_till_end) {
    x = chars_till_end;
    x -= buffer_row_has_whitespace_at_position(current_line, line_length);
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

static void editor_move_cursor_to_end(Editor* editor) {
  editor_move_cursor_x(editor,
                       buffer_row_get_length(editor->current_buffer->current_row));
}

static void editor_process_editor_key(Editor* editor, int key) {
  // TODO: scroll buffers
  Buffer* current_buffer = editor->current_buffer;
  switch (key) {
    case 'h': {
      // Move cursor left
      editor->end_line_mode = false;
      if (editor->cursor.x > editor->number_of_line_digits) {
        editor->cursor.x--;
      } else {
        // scroll buffer to the right
        if (current_buffer && current_buffer->start_column > 0) {
          current_buffer->start_column--;
        }
      }
      return;
    }
    case 'l': {
      // Move cursor right
      editor_move_cursor_x(editor, 1);
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
    }
    case 'w': {
      int offset_to_word = buffer_row_get_offset_to_next_word(
        editor->current_buffer->current_row,
        editor->cursor.x + editor->current_buffer->start_column);
      editor_move_cursor_x(editor, offset_to_word);
    }
    default: {
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

void editor_process_key(Editor* editor, int key) {
  bool done = false;
  mvaddch(editor->window.height - 1, editor->window.width - 2, (char)key);
  while (!done) {
    switch (editor->state) {
      case EditorState_CollectingCommand: {
        if (!editor_collect_command(editor, key)) {
          return;
        }
      } break;
      case EditorState_ProcessingCommand: {
        int command_result = editor_process_command(&editor->command);
        switch (command_result) {
          case CommandResult_ShouldExit: {
            editor->state = EditorState_Exiting;
          } break;
          case CommandResult_CommandNotFound: {
            editor_set_error_message(editor, "Command not found");
            editor->state = EditorState_Running;
          } break;
          default: {
          }
        }
        command_deinit(&editor->command);
        return;
      } break;
      case EditorState_Running:
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
            editor_process_editor_key(editor, key);
            move(editor->cursor.y, editor->cursor.x);
            return;
          }
        }
        break;
      case EditorState_EditMode:
        editor_restore_cursor_position(editor);
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