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

#include "toolbar_widget.h"

#include <ncurses.h>

typedef struct Toolbar {
  int y_position;      // Screen row where toolbar starts
  int height;          // Number of rows (1 or 2)
  int width;           // Screen width

  // Widget lists by position
  Widget* left_widgets;
  Widget* right_widgets;
  Widget* center_widgets;

  // Background attributes
  attr_t background_attr;

  // Dirty flag
  bool needs_redraw;

  // Debug mode
  bool debug_mode;
} Toolbar;

// Toolbar lifecycle
void toolbar_init(Toolbar* toolbar, int window_height, int window_width);
void toolbar_deinit(Toolbar* toolbar);

// Widget management
bool toolbar_add_widget(Toolbar* toolbar, Widget* widget);
void toolbar_remove_widget(Toolbar* toolbar, const char* name);
Widget* toolbar_find_widget(Toolbar* toolbar, const char* name);

// Layout and rendering
void toolbar_layout(Toolbar* toolbar);
void toolbar_draw(Toolbar* toolbar, void* editor);
void toolbar_invalidate(Toolbar* toolbar);

// Resize handling
void toolbar_resize(Toolbar* toolbar, int window_height, int window_width);

// Update all widgets (editor should be Editor*)
void toolbar_update(Toolbar* toolbar, void* editor);
