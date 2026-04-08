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

#include "buffer.h"

typedef struct {
  char* pattern;
  int last_match_line;  // 0-based line number of last match
  int last_match_col;   // 0-based column of last match
  int origin_line;      // where the search started (for wrap detection)
  int origin_col;       // where the search started (for wrap detection)
  bool forward;         // requested search direction for next/prev
  bool last_direction;  // actual direction of last successful search
  bool case_sensitive;
  bool has_matched;     // true if a match has been found (for n/N navigation)
} SearchState;

typedef struct {
  int replacements;
  int lines_affected;
} ReplaceResult;

void search_init(SearchState* state);
void search_deinit(SearchState* state);
void search_set_pattern(SearchState* state, const char* pattern, bool forward);
void search_clear_pattern(SearchState* state);

// Find next occurrence in the given direction
// Returns: true if found, false otherwise
// Updates last_match_line and last_match_col on success
bool search_find_next(SearchState* state, Buffer* buffer);
bool search_find_prev(SearchState* state, Buffer* buffer);

// Set starting position for search
void search_set_position(SearchState* state, int line, int col);

// Replace functions
// Replace first occurrence on a specific line (0-based)
// If global is true, replace all occurrences on that line
ReplaceResult search_replace_line(Buffer* buffer,
                                  int line_num,
                                  const char* pattern,
                                  const char* replacement,
                                  bool global);

// Replace in entire buffer
// If global is true, replace all occurrences per line
ReplaceResult search_replace_all(Buffer* buffer,
                                 const char* pattern,
                                 const char* replacement,
                                 bool global);

// Replace in line range [start_line, end_line] (0-based, inclusive)
ReplaceResult search_replace_range(Buffer* buffer,
                                   int start_line,
                                   int end_line,
                                   const char* pattern,
                                   const char* replacement,
                                   bool global);

// Helper: find pattern in string starting at position
// Returns: position of match or -1 if not found
int search_find_in_string(const char* haystack,
                          const char* needle,
                          int start_pos,
                          bool case_sensitive);
