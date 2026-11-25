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

// Paths and the file operations of the file tree and :e, without any
// terminal I/O. Every char* returned is malloc'd; the caller frees it.

// "dir/name", without a doubled slash when dir is "/" or ends in one.
char* path_join(const char* dir, const char* name);

// ~ and ~/x become $HOME and $HOME/x; anything else is copied.
char* path_expand_home(const char* path);

// An absolute path with "//", "." and ".." taken out by text (symlinks are
// not followed): a relative path is taken from the working directory.
char* path_absolute(const char* path);

// Whether two names reach the same path (path_absolute of both).
bool path_same(const char* a, const char* b);

// Whether `path` is `dir` or lies below it (both made absolute).
bool path_is_within(const char* path, const char* dir);

// Everything before the last slash ("/" for "/x", "." for "x").
char* path_dirname(const char* path);

// The part after the last slash.
const char* path_basename(const char* path);

// `name` taken from `dir` when relative (after ~ expansion), as it is when
// absolute.
char* path_resolve(const char* dir, const char* name);

// The operations: NULL on success, otherwise a message for the status line
// (static storage, valid until the next call). None of them overwrites an
// existing path.
const char* fileops_create_file(const char* path);
const char* fileops_make_directory(const char* path);  // parents too
const char* fileops_rename(const char* from, const char* to);
const char* fileops_copy(const char* from, const char* to);  // directories too
const char* fileops_remove(const char* path);  // directories with everything in them

// File name completion for the command line: the names that `partial`
// could become, sorted, each the whole completed text (a directory ends in
// '/'). Hidden names only when the last part starts with '.'. Returns how
// many (the array is NULL when none).
int path_complete(const char* partial, char*** matches);
// The same, with a relative `partial` taken from directory `base` (NULL: the
// working directory); the matches still start with `partial` as typed.
int path_complete_in(const char* base, const char* partial, char*** matches);
void path_matches_free(char** matches, int count);

// The longest text every match starts with (malloc'd).
char* path_common_prefix(char** matches, int count);
