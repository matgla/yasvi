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

/* This wrapper includes editor.c with mock ncurses instead of real ncurses */

/* First include our mock ncurses to define all the types and functions */
#include "mock_ncurses.h"

/* Define the macros that window.h expects */
#define COLOR_KEYWORD (COLOR_PAIR(1))
#define COLOR_STRING (COLOR_PAIR(2))
#define COLOR_COMMENT (COLOR_PAIR(3) | A_ITALIC)
#define COLOR_TYPE (COLOR_PAIR(4))
#define COLOR_NUMBER (COLOR_PAIR(5))
#define COLOR_PREPROCESSOR (COLOR_PAIR(6))
#define COLOR_OTHER (A_NORMAL)

/* Mock WINDOW structure */
typedef struct {
  int width;
  int height;
} WINDOW;

static inline void window_init(WINDOW* window) {
  window->width = 80;
  window->height = 24;
}

static inline void window_redraw_screen(const WINDOW* window) {
  (void)window;
}

static inline void window_deinit(WINDOW* window) {
  (void)window;
}

/* Include highlight.h */
#include "highlight.h"

/* Prevent editor.h from including the real window.h */
#define WINDOW_INCLUDED
#define WINDOW MockWindow

/* Now we can include the editor implementation */
#include "../editor.c"
