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

int main() {
  int key = 0;
  Editor editor = {
      .state = EditorState_Running,
  };
  window_init();
  while (true) {
    if ((key = getch()) != ERR) {
      editor_process_key(&editor, key);
    }

    if (editor_should_exit(&editor)) {
      break;
    }
    window_redraw_screen();
  }
  window_deinit();
  return 0;
}