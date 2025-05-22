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

#define SHOULD_EXIT 2

static void editor_collect_command(Editor *editor, int key) {
  if (key == '\n') {
    editor->state = EditorState_ProcessingCommand;
    return;
  }
  if (key == 27) {
    // cancel command
    command_deinit(&editor->command);
    editor->state = EditorState_Running;
    return;
  }
}

int editor_process_command(const Command *command) {
  printf("Processing command: '%s'\n", command->buffer);
  if (strcmp(command->buffer, "q") == 0) {
    return SHOULD_EXIT;
  }
  return 0;
}

void editor_process_key(Editor *editor, int key) {
  mvaddch(40, 70, (char)key);
  switch (editor->state) {
  case EditorState_CollectingCommand: {
    return editor_collect_command(editor, key);
  } break;
  case EditorState_ProcessingCommand: {
    if (editor_process_command(&editor->command) == SHOULD_EXIT) {
      editor->state = EditorState_Exiting;
      command_deinit(&editor->command);
      return;
    }
  } break;
  case EditorState_Running:
    switch (key) {
    case ':': {
      command_init(&editor->command);
      editor->state = EditorState_CollectingCommand;
    } break;
    default:
      break;
    }
    break;
  case EditorState_Exiting:
    break;
  };
}

bool editor_should_exit(const Editor *editor) {
  return editor->state == EditorState_Exiting;
}