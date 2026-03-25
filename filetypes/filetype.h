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
#include <stddef.h>

#include "../highlight.h"

// Forward declarations
typedef struct BufferRow BufferRow;

// Highlighter context passed to each highlight_line call
typedef struct HighlightContext {
  const char* line;    // Line content
  int len;             // Line length
  char* tokens;        // Output: EHighlightToken per char

  // Multiline state (inherited from previous lines)
  bool in_multiline_comment;
  bool in_string;
  char string_char;  // '"' or '\'' for tracking string type
} HighlightContext;

// Highlighter interface for a specific language
typedef struct Highlighter {
  const char* name;  // "c", "shell", etc.
  void (*highlight_line)(HighlightContext* ctx);
} Highlighter;

// Filetype definition
typedef struct Filetype {
  const char* name;        // "c", "shell", "python", etc.
  const char** extensions; // NULL-terminated array of extensions
  const char** shebangs;   // NULL-terminated array of shebangs (optional)
  const Highlighter* highlighter;
} Filetype;

// Lookup functions
const Filetype* filetype_by_extension(const char* filename);
const Filetype* filetype_by_shebang(const char* first_line);
const Filetype* filetype_detect(const char* filename, const char* first_line);

// Highlight a buffer row using its filetype
void buffer_row_highlight_with_filetype(BufferRow* row, const Filetype* filetype);

// Built-in filetypes (defined in their respective files)
extern const Filetype g_filetype_c;
extern const Filetype g_filetype_shell;

// Registry of all built-in filetypes
extern const Filetype* g_filetypes[];
extern const size_t g_num_filetypes;
