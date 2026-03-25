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

#include "mock_ncurses.h"

#include <stdarg.h>
#include <string.h>

#define MAX_STRING_LEN 256

static int last_y = 0;
static int last_x = 0;
static char last_string[MAX_STRING_LEN] = {0};
static attr_t current_attr = A_NORMAL;

WINDOW mock_stdscr = {24, 80};
WINDOW* stdscr = &mock_stdscr;

void mock_ncurses_reset(void) {
  last_y = 0;
  last_x = 0;
  last_string[0] = '\0';
  current_attr = A_NORMAL;
  mock_stdscr._maxy = 24;
  mock_stdscr._maxx = 80;
}

int mock_ncurses_get_last_y(void) {
  return last_y;
}

int mock_ncurses_get_last_x(void) {
  return last_x;
}

const char* mock_ncurses_get_last_string(void) {
  return last_string;
}

/* Mock implementations */

int move(int y, int x) {
  last_y = y;
  last_x = x;
  return 0;
}

int mvaddch(int y, int x, chtype ch) {
  (void)ch;
  last_y = y;
  last_x = x;
  return 0;
}

int mvaddstr(int y, int x, const char* str) {
  last_y = y;
  last_x = x;
  strncpy(last_string, str, MAX_STRING_LEN - 1);
  last_string[MAX_STRING_LEN - 1] = '\0';
  return 0;
}

int mvaddnstr(int y, int x, const char* str, int n) {
  last_y = y;
  last_x = x;
  if (n >= MAX_STRING_LEN) n = MAX_STRING_LEN - 1;
  strncpy(last_string, str, n);
  last_string[n] = '\0';
  return 0;
}

int clrtoeol(void) {
  return 0;
}

int attrset(attr_t attrs) {
  current_attr = attrs;
  return 0;
}

int addch(chtype ch) {
  (void)ch;
  return 0;
}

int curs_set(int visibility) {
  (void)visibility;
  return 0;
}

int clear(void) {
  return 0;
}

int cbreak(void) {
  return 0;
}

int raw(void) {
  return 0;
}

int keypad(void* win, bool flag) {
  (void)win;
  (void)flag;
  return 0;
}

int nodelay(void* win, bool flag) {
  (void)win;
  (void)flag;
  return 0;
}

int noecho(void) {
  return 0;
}

void getmaxyx(void* win, int y, int x) {
  (void)win;
  (void)y;
  (void)x;
}

WINDOW* initscr(void) {
  return stdscr;
}

int endwin(void) {
  return 0;
}

int refresh(void) {
  return 0;
}

int getmaxy(WINDOW* win) {
  return win ? win->_maxy : 24;
}

int getmaxx(WINDOW* win) {
  return win ? win->_maxx : 80;
}

int start_color(void) {
  return 0;
}

int init_pair(short pair, short f, short b) {
  (void)pair;
  (void)f;
  (void)b;
  return 0;
}

int getcury(void* win) {
  (void)win;
  return last_y;
}

int getcurx(void* win) {
  (void)win;
  return last_x;
}

/* mvprintw mock - simplified version */
int mvprintw(int y, int x, const char* fmt, ...) {
  (void)fmt;
  last_y = y;
  last_x = x;
  return 0;
}
