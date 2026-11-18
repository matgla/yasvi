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

#include "window.h"

#include <stdbool.h>
#include <termios.h>
#include <unistd.h>

#include <ncurses.h>

// raw() as X/Open has it: ^C is a key, not SIGINT. yasos_curses' raw()
// leaves ISIG on, and YasOS's console sends ^C to the foreground process,
// which is vi while it edits: Ctrl-C killed vi. The :term panel uses it to
// interrupt what its shell runs. endwin() puts the saved termios back.
static void window_keep_ctrl_c_a_key(void) {
  // Both descriptors, as yasos_curses sets both (one tty on YasOS).
  const int fds[2] = {0, 1};  // stdin, stdout
  for (int i = 0; i < 2; ++i) {
    struct termios mode;
    if (tcgetattr(fds[i], &mode) == 0 && (mode.c_lflag & ISIG) != 0) {
      mode.c_lflag &= ~ISIG;
      tcsetattr(fds[i], TCSANOW, &mode);
    }
  }
}

void window_init(Window* window) {
  initscr();
  cbreak();
  raw();
  keypad(stdscr, true);
  nodelay(stdscr, true);
  noecho();
  clear();
  refresh();
  getmaxyx(stdscr, window->height, window->width);
  start_color();
  init_pair(1, COLOR_RED, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, 8, COLOR_BLACK);
  init_pair(4, COLOR_YELLOW, COLOR_BLACK);
  init_pair(5, COLOR_BLUE, COLOR_BLACK);
  init_pair(6, COLOR_CYAN, COLOR_BLACK);
  init_pair(7, COLOR_RED, COLOR_BLACK);
  // Last: yasos_curses writes its own copy of the termios back on noecho()
  // and friends.
  window_keep_ctrl_c_a_key();
}

void window_deinit(Window* window) {
  (void)window;
  endwin();
}

void window_redraw_screen(const Window* window) {
  (void)window;
  refresh();
}