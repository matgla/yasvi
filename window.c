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

#include <ncurses.h>

void window_init(Window* window) {
  initscr();
  cbreak();
  raw();
  keypad(stdscr, true);
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
}

void window_deinit(Window* window) {
  (void)window;
  endwin();
}

void window_redraw_screen(const Window* window) {
  (void)window;
  refresh();
}