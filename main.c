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

#include <ncurses.h>
#include <stdio.h>

#include "editor.h"
#include "window.h"

int main(int argc, char* argv[]) {
  int key = 0;
  Editor editor = {
    .state = EditorState_Running,
    .window = {0, 0},
    .command = {NULL, 0, 0},
    .error_message = NULL,
    .cursor = {2, 0},
    .number_of_line_digits = 3,
    .current_buffer = NULL,
    .buffers = NULL,
    .number_of_buffers = 0,
    .end_line_mode = false,
    .status_bar = NULL,
    .key_sequence = {0},
    .repeat_count = 0,
  };
  editor_init(&editor);
  if (argc > 1) {
    editor_load_file(&editor, argv[1]);
  }

  editor_redraw_screen(&editor);
  while (true) {
    if ((key = getch()) != ERR) {
      editor_process_key(&editor, key);
    }

    if (editor_should_exit(&editor)) {
      break;
    }
    editor_redraw_screen(&editor);
  }
  editor_deinit(&editor);
  return 0;
}
