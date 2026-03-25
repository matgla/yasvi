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

// Standard widgets
Widget* widget_mode_create(void);
Widget* widget_filename_create(void);
Widget* widget_position_create(void);
Widget* widget_command_create(void);
Widget* widget_message_create(void);
Widget* widget_keyseq_create(void);

// Debug keystroke widget configuration
typedef struct {
  bool show_ascii_char;    // Show printable character representation
  bool show_decimal_code;  // Show decimal key code
  bool show_hex_code;      // Show hexadecimal key code
  bool show_octal_code;    // Show octal key code
  bool compact_mode;       // Single char display vs full info
} DebugKeystrokeConfig;

Widget* widget_debug_keystroke_create(const DebugKeystrokeConfig* config);
void widget_debug_keystroke_add_key(Widget* widget, int key);
void widget_debug_keystroke_clear(Widget* widget);
