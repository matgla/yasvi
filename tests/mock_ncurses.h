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

#pragma once

#include <stdbool.h>

/* Mock ncurses types and constants for testing */

typedef unsigned int chtype;
typedef chtype attr_t;

/* Key codes */
#define KEY_MIN 0401
#define KEY_DOWN 0402
#define KEY_UP 0403
#define KEY_LEFT 0404
#define KEY_RIGHT 0405
#define KEY_BACKSPACE 0407
#define KEY_DC 0512          // Delete character
#define KEY_HOME 0520        // Home key
#define KEY_END 0527         // End key
#define KEY_PPAGE 0523       // Page up
#define KEY_NPAGE 0522       // Page down
#define KEY_IC 0513          // Insert char

/* Attributes */
#define A_NORMAL 0
#define A_BOLD 1
#define A_ITALIC 2
#define A_DIM 4

/* Color pair macro */
#define COLOR_PAIR(n) ((n) << 8)

/* Colors */
#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_CYAN 5

/* Mock ncurses functions */
int move(int y, int x);
int mvaddch(int y, int x, chtype ch);
int mvaddstr(int y, int x, const char* str);
int mvaddnstr(int y, int x, const char* str, int n);
int clrtoeol(void);
int attrset(attr_t attrs);
int addch(chtype ch);
int curs_set(int visibility);
int getcury(void* win);
int getcurx(void* win);
int clear(void);
int cbreak(void);
int raw(void);
int keypad(void* win, bool flag);
int nodelay(void* win, bool flag);
int noecho(void);
void getmaxyx(void* win, int y, int x);

/* Mock window operations */
typedef struct {
  int _maxy;
  int _maxx;
} WINDOW;

extern WINDOW* stdscr;

WINDOW* initscr(void);
int endwin(void);
int refresh(void);
int getmaxy(WINDOW* win);
int getmaxx(WINDOW* win);

/* Mock color initialization */
int start_color(void);
int init_pair(short pair, short f, short b);

/* mvprintw */
int mvprintw(int y, int x, const char* fmt, ...);

/* Test helpers */
void mock_ncurses_reset(void);
int mock_ncurses_get_last_y(void);
int mock_ncurses_get_last_x(void);
const char* mock_ncurses_get_last_string(void);
