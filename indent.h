/*
 Copyright (c) 2026 Mateusz Stadnik

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

#include "buffer_row.h"

// Automatic indentation (the `autoindent` setting, config.h), no curses:
// the editor asks these what a new line or a closing bracket should be
// indented with.
//
// Every file keeps the indentation of the line above. Files in a language
// with brackets (C, Zig, PIO, shell) also get one level more after a line
// that opens a block -- it ends with { ( or [, or for shell with then, do,
// else or in -- and a closing } ) ] is put under the line that opened it.
// Strings, character literals and line comments are skipped when brackets
// are matched; block comments are not.

typedef enum {
  IndentLanguage_None,     // plain text: keep the indentation of the line above
  IndentLanguage_Brackets,  // C, Zig, PIO
  IndentLanguage_Shell,     // brackets and then/do/else/in
} IndentLanguage;

// The rules for a filetype name ("c", "zig", "pio", "shell"), None for NULL
// or anything else.
IndentLanguage indent_language(const char* filetype_name);

// The number of spaces and tabs `text` starts with.
int indent_leading(const char* text, int length);

// Whether `text` (the part of a line before the cursor) opens a block, so
// the next line is indented one level more.
bool indent_opens_block(IndentLanguage language, const char* text, int length);

// Whether the first character of `text` that is not white space is a
// closing bracket.
bool indent_starts_with_closer(const char* text, int length);

// The row with the bracket that the closing bracket at `column` of `row`
// closes, or NULL when there is none within the lines searched.
const BufferRow* indent_find_opener(IndentLanguage language,
                                    const BufferRow* row,
                                    int column);

// One level of indentation for the file starting at `first` into `out`
// (NUL-terminated, at most size - 1 characters): a tab when the file is
// indented with tabs, else the step its lines are indented by, else
// `tab_size` spaces. Returns its length.
int indent_unit(const BufferRow* first, int tab_size, char* out, int size);
