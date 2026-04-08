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

#define _GNU_SOURCE
#include "search.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer_row.h"

void search_init(SearchState* state) {
  if (state == NULL) {
    return;
  }
  state->pattern = NULL;
  state->last_match_line = 0;
  state->last_match_col = 0;
  state->origin_line = 0;
  state->origin_col = 0;
  state->forward = true;
  state->last_direction = true;
  state->case_sensitive = true;
  state->has_matched = false;
}

void search_deinit(SearchState* state) {
  if (state == NULL) {
    return;
  }
  search_clear_pattern(state);
}

void search_set_pattern(SearchState* state, const char* pattern, bool forward) {
  if (state == NULL) {
    return;
  }
  search_clear_pattern(state);
  if (pattern != NULL) {
    state->pattern = strdup(pattern);
  }
  state->forward = forward;
  // Reset search state for new pattern
  state->has_matched = false;
  state->last_direction = forward;
}

void search_clear_pattern(SearchState* state) {
  if (state == NULL) {
    return;
  }
  if (state->pattern != NULL) {
    free(state->pattern);
    state->pattern = NULL;
  }
}

void search_set_position(SearchState* state, int line, int col) {
  if (state == NULL) {
    return;
  }
  state->last_match_line = line;
  state->last_match_col = col;
  // Also set origin - this is where the search starts from
  state->origin_line = line;
  state->origin_col = col;
}

// Helper: case-insensitive string comparison
static int case_insensitive_strncmp(const char* s1,
                                    const char* s2,
                                    size_t n) {
  for (size_t i = 0; i < n; i++) {
    char c1 = tolower((unsigned char)s1[i]);
    char c2 = tolower((unsigned char)s2[i]);
    if (c1 != c2) {
      return c1 - c2;
    }
    if (c1 == '\0') {
      return 0;
    }
  }
  return 0;
}

int search_find_in_string(const char* haystack,
                          const char* needle,
                          int start_pos,
                          bool case_sensitive) {
  if (haystack == NULL || needle == NULL || start_pos < 0) {
    return -1;
  }

  size_t haystack_len = strlen(haystack);
  size_t needle_len = strlen(needle);

  if (needle_len == 0 || start_pos >= (int)haystack_len || needle_len > haystack_len) {
    return -1;
  }

  for (size_t i = start_pos; i <= haystack_len - needle_len; i++) {
    int cmp;
    if (case_sensitive) {
      cmp = strncmp(&haystack[i], needle, needle_len);
    } else {
      cmp = case_insensitive_strncmp(&haystack[i], needle, needle_len);
    }
    if (cmp == 0) {
      return (int)i;
    }
  }

  return -1;
}

bool search_find_next(SearchState* state, Buffer* buffer) {
  if (state == NULL || buffer == NULL || state->pattern == NULL) {
    return false;
  }

  int num_lines = buffer_get_number_of_lines(buffer);
  if (num_lines == 0) {
    return false;
  }

  // Start from last match position (or origin if no match yet)
  int start_line = state->last_match_line;
  int start_col = state->last_match_col;
  
  // If we already found a match, skip past it to find the next one
  if (state->has_matched) {
    start_col = state->last_match_col + 1;
  }

  // Search from start_line to end
  for (int line = start_line; line < num_lines; line++) {
    BufferRow* row = buffer_get_row(buffer, line);
    if (row == NULL || row->data == NULL) {
      continue;
    }

    int search_from = (line == start_line) ? start_col : 0;

    int col = search_find_in_string(
      row->data, state->pattern, search_from, state->case_sensitive);

    if (col >= 0) {
      state->last_match_line = line;
      state->last_match_col = col;
      state->forward = true;
      state->last_direction = true;
      state->has_matched = true;
      return true;
    }
  }

  // Wrap around: search lines [0, start_line) from col 0
  for (int line = 0; line < start_line && line < num_lines; line++) {
    BufferRow* row = buffer_get_row(buffer, line);
    if (row == NULL || row->data == NULL) {
      continue;
    }

    int col = search_find_in_string(row->data, state->pattern, 0, state->case_sensitive);

    if (col >= 0) {
      state->last_match_line = line;
      state->last_match_col = col;
      state->forward = true;
      state->last_direction = true;
      state->has_matched = true;
      return true;
    }
  }
  
  // Special case: search [0, start_col) on start_line
  // This handles the case where we started from middle of line and need to wrap
  if (start_col > 0) {
    BufferRow* row = buffer_get_row(buffer, start_line);
    if (row != NULL && row->data != NULL) {
      int col = search_find_in_string(row->data, state->pattern, 0, state->case_sensitive);
      if (col >= 0 && col < start_col) {
        state->last_match_line = start_line;
        state->last_match_col = col;
        state->forward = true;
        state->last_direction = true;
        state->has_matched = true;
        return true;
      }
    }
  }

  return false;
}

bool search_find_prev(SearchState* state, Buffer* buffer) {
  if (state == NULL || buffer == NULL || state->pattern == NULL) {
    return false;
  }

  int num_lines = buffer_get_number_of_lines(buffer);
  if (num_lines == 0) {
    return false;
  }

  // Start from last match position (or origin if no match yet)
  int start_line = state->last_match_line;
  int start_col = state->last_match_col;
  size_t pattern_len = strlen(state->pattern);

  // On initial search (has_matched == false), include current position
  // On subsequent searches, skip the current match to find the previous one
  int search_end_col = start_col + 1;  // +1 to include start_col
  if (state->has_matched) {
    search_end_col = start_col;  // Skip current match on subsequent searches
  }

  // Search from start_line to beginning
  for (int line = start_line; line >= 0; line--) {
    BufferRow* row = buffer_get_row(buffer, line);
    if (row == NULL || row->data == NULL) {
      continue;
    }

    int row_len = (int)strlen(row->data);
    
    // Determine search range (exclusive end)
    int search_end = row_len + 1;  // +1 to include last character
    if (line == start_line) {
      // On starting line, only search strictly before current position
      search_end = search_end_col;
    }

    // Find the last occurrence before search_end
    int last_match = -1;
    
    // Iterate through all possible positions to find the last valid match
    for (int pos = 0; pos <= row_len - (int)pattern_len; pos++) {
      // Quick bounds check before calling strncmp
      if (pos + pattern_len > (size_t)row_len) {
        break;
      }
      
      int col = search_find_in_string(
        row->data, state->pattern, pos, state->case_sensitive);
      if (col < 0) {
        break;
      }
      // Only accept matches strictly before search_end
      if (col < search_end) {
        last_match = col;
        pos = col;  // Continue searching from after this match
      }
      // Continue searching even if col >= search_end, there might be earlier matches
    }

    if (last_match >= 0) {
      state->last_match_line = line;
      state->last_match_col = last_match;
      state->forward = false;
      state->last_direction = false;
      state->has_matched = true;
      return true;
    }
  }

  // Wrap around: search from end to start_line (exclusive)
  for (int line = num_lines - 1; line > start_line; line--) {
    BufferRow* row = buffer_get_row(buffer, line);
    if (row == NULL || row->data == NULL) {
      continue;
    }

    int row_len = (int)strlen(row->data);

    // Find the last occurrence
    int last_match = -1;
    
    for (int pos = 0; pos <= row_len - (int)pattern_len; pos++) {
      int col = search_find_in_string(
        row->data, state->pattern, pos, state->case_sensitive);
      if (col < 0) {
        break;
      }
      last_match = col;
      pos = col;
    }

    if (last_match >= 0) {
      state->last_match_line = line;
      state->last_match_col = last_match;
      state->forward = false;
      state->last_direction = false;
      state->has_matched = true;
      return true;
    }
  }

  // Special case: single-line buffer with multiple occurrences
  if (start_line == 0 && num_lines == 1) {
    BufferRow* row = buffer_get_row(buffer, 0);
    if (row != NULL && row->data != NULL) {
      int row_len = (int)strlen(row->data);
      int last_match = -1;
      
      for (int pos = 0; pos <= row_len - (int)pattern_len; pos++) {
        int col = search_find_in_string(
          row->data, state->pattern, pos, state->case_sensitive);
        if (col < 0) {
          break;
        }
        last_match = col;
        pos = col;
      }
      
      if (last_match >= 0) {
        state->last_match_line = 0;
        state->last_match_col = last_match;
        state->forward = false;
        state->last_direction = false;
        state->has_matched = true;
        return true;
      }
    }
  }

  return false;
}

// Helper: replace first occurrence in a row starting at position
// Returns: new position after replacement, or -1 if no replacement
static int replace_in_row(BufferRow* row,
                          const char* pattern,
                          const char* replacement,
                          int start_pos,
                          bool case_sensitive) {
  if (row == NULL || row->data == NULL || pattern == NULL ||
      replacement == NULL) {
    return -1;
  }

  int match_pos =
    search_find_in_string(row->data, pattern, start_pos, case_sensitive);
  if (match_pos < 0) {
    return -1;
  }

  size_t pattern_len = strlen(pattern);
  size_t replacement_len = strlen(replacement);
  int row_len = (int)strlen(row->data);

  // Calculate new row length
  int new_len = row_len - (int)pattern_len + (int)replacement_len;

  // Allocate new data buffer if needed
  char* new_data;
  if (new_len >= row->allocated_size) {
    new_data = (char*)malloc(new_len + 1);
    if (new_data == NULL) {
      return -1;
    }
  } else {
    new_data = row->data;
  }

  // Build new string
  // Copy prefix (before match)
  if (new_data != row->data) {
    memcpy(new_data, row->data, match_pos);
  }
  // Copy replacement
  memcpy(new_data + match_pos, replacement, replacement_len);
  // Copy suffix (after match)
  memcpy(new_data + match_pos + replacement_len,
         row->data + match_pos + pattern_len,
         row_len - match_pos - pattern_len + 1);  // +1 for null terminator

  // Update row
  if (new_data != row->data) {
    free(row->data);
    free(row->highlight_data);
    row->data = new_data;
    row->allocated_size = new_len + 1;
    row->highlight_data = (char*)malloc(row->allocated_size);
    if (row->highlight_data) {
      memset(row->highlight_data, 0, row->allocated_size);
    }
  }

  row->len = new_len;
  row->dirty = true;

  // Re-highlight the row
  buffer_row_highlight_line(row);

  return match_pos + (int)replacement_len;
}

ReplaceResult search_replace_line(Buffer* buffer,
                                  int line_num,
                                  const char* pattern,
                                  const char* replacement,
                                  bool global) {
  ReplaceResult result = {0, 0};

  if (buffer == NULL || pattern == NULL || replacement == NULL) {
    return result;
  }

  BufferRow* row = buffer_get_row(buffer, line_num);
  if (row == NULL) {
    return result;
  }

  int pos = 0;
  bool replaced = false;

  do {
    int new_pos = replace_in_row(row, pattern, replacement, pos, true);
    if (new_pos < 0) {
      break;
    }
    result.replacements++;
    replaced = true;
    pos = new_pos;
  } while (global);

  if (replaced) {
    result.lines_affected = 1;
  }

  return result;
}

ReplaceResult search_replace_all(Buffer* buffer,
                                 const char* pattern,
                                 const char* replacement,
                                 bool global) {
  int num_lines = buffer_get_number_of_lines(buffer);
  return search_replace_range(buffer, 0, num_lines - 1, pattern, replacement,
                              global);
}

ReplaceResult search_replace_range(Buffer* buffer,
                                   int start_line,
                                   int end_line,
                                   const char* pattern,
                                   const char* replacement,
                                   bool global) {
  ReplaceResult result = {0, 0};

  if (buffer == NULL || pattern == NULL || replacement == NULL) {
    return result;
  }

  int num_lines = buffer_get_number_of_lines(buffer);
  if (start_line < 0) {
    start_line = 0;
  }
  if (end_line >= num_lines) {
    end_line = num_lines - 1;
  }
  if (start_line > end_line) {
    return result;
  }

  for (int line = start_line; line <= end_line; line++) {
    ReplaceResult line_result =
      search_replace_line(buffer, line, pattern, replacement, global);
    result.replacements += line_result.replacements;
    result.lines_affected += line_result.lines_affected;
  }

  return result;
}
