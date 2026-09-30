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

#include "buffer.h"

/* One line of a ctags "tags" file: where a name is defined.
 *
 * The address is either a line number (ctags --excmd=number) or a search
 * pattern (the default, /^text$/), which survives edits above the definition.
 * ctags cuts patterns at 96 characters and then drops the `$`, so a pattern
 * without `$` matches a line that starts with it. */
typedef struct {
  char* name;
  char* filename;  // relative to the directory of the tags file, as resolved
  int line;        // 1-based; 0 when the address is a pattern
  char* pattern;   // unescaped text between the delimiters, without ^ and $
  bool anchor_start;
  bool anchor_end;
} TagEntry;

/* Finds a "tags" file for a file being edited: the file's own directory
 * first, then each parent up to the root, then the working directory.
 * Returns a malloc'd path or NULL. */
char* tags_find_file(const char* edited_path);

/* Parses one tags-file line. `tags_dir` (may be NULL or "") is prefixed to a
 * relative filename. Returns false for comment (!_TAG_) and malformed lines. */
bool tags_parse_line(const char* line, const char* tags_dir, TagEntry* out);

/* Collects every entry named `name` from the tags file at `tags_path`.
 * Returns the count (0 when none or the file cannot be read) and stores a
 * malloc'd array in *out, to be released with tags_free_entries. */
int tags_lookup(const char* tags_path, const char* name, TagEntry** out);

/* Calls `visit` with the name of every tag starting with `prefix`, in file
 * order (a name defined in several places comes once per definition). Stops
 * early when `visit` returns false. Returns the number of names visited. */
int tags_for_each_name(const char* tags_path,
                       const char* prefix,
                       bool (*visit)(void* context, const char* name, size_t len),
                       void* context);

void tags_free_entries(TagEntry* entries, int count);

/* The 0-based line of `buffer` that `entry` points at, or -1. */
int tags_find_line(const TagEntry* entry, const Buffer* buffer);
