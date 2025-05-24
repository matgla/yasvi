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

static void editor_clear_error_message(Editor *editor)
{
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
    (char*)malloc(editor->command.cursor_position + error_length + 4);
  if (editor->error_message) {
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
          default:
            return;
        }
        break;
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
    mvaddch(editor->window.height - 1, 0, ':');
    mvaddstr(editor->window.height - 1, 1, editor->command.buffer);
    return;
  }
  if (editor->error_message) {
    mvaddstr(editor->window.height - 1, 1, editor->error_message);
    return;
  }
}

void editor_redraw_screen(const Editor* editor) {
  editor_draw_status_bar(editor);
  window_redraw_screen(&editor->window);
}

void editor_init(Editor* editor) {
  window_init(&editor->window);
}

void editor_deinit(Editor* editor) {
  if (editor->error_message) {
    free(editor->error_message);
    editor->error_message = NULL;
  }
  window_deinit(&editor->window);
}