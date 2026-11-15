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
#include <stddef.h>

// Manual page lookup for :man and K, the same search as /usr/bin/man
// (find_page and in_dir there):
//   1. for each DIR of the MANPATH (colon-separated): DIR/NAME.txt, else
//      DIR/NAME.txt.gz (the built pages are gzipped; one written on the board
//      can be plain)
//   2. for each DIR: the first line "NAME PAGE" of DIR/aliases, if DIR/PAGE.txt
//      or DIR/PAGE.txt.gz exists (fprintf -> printf, sys/stat.h -> sys-stat.h)
// DIR/whatis is the one-line index of every page (plain :man). whatis and
// aliases are always plain text.
//
// After the two files, DIR/index is searched: the built pages are packed into
// xz blocks of about 64 KB (scripts/man/build_man.py), and each index line is
// "NAME BLOCK OFFSET LENGTH". Such a page is LENGTH bytes at OFFSET of the
// unpacked DIR/BLOCK. The order per directory is NAME.txt, NAME.txt.gz, index.
//
// A .gz page is shown from a copy zcat writes to man_temp_path's file; a
// packed page from its block, unpacked there by xzcat and cut with man_cut.

#define MAN_DEFAULT_PATH "/usr/share/man"
#define MAN_TEMP_DIR "/tmp"
#define MAN_TEMP_PREFIX "yasvi-man-"

// Where a page is: the file `file` itself (offset < 0), or `length` bytes at
// `offset` of the unpacked xz block `file`. `name` is the page's own name
// (printf for fprintf). Both strings malloc'd; free with man_page_free.
typedef struct {
  char* file;
  char* name;
  long offset;
  long length;
} ManPage;

// Finds the page for `name` in `manpath` (NULL or "" = MAN_DEFAULT_PATH).
bool man_locate(const char* manpath, const char* name, ManPage* out);
void man_page_free(ManPage* page);

// The file holding the page for `name` (its own file or its xz block),
// malloc'd, or NULL when there is none.
char* man_find_page(const char* manpath, const char* name);

// Keeps only `length` bytes at `offset` of the file at `path` (an unpacked
// block becomes the page). False if the file is shorter or cannot be
// rewritten.
bool man_cut(const char* path, long offset, long length);

// Whether `path` is a gzipped page (ends in ".gz").
bool man_is_gzipped(const char* path);

// Where a gzipped `page` is unpacked: TEMP_DIR/yasvi-man-PAGE.txt, PAGE being
// the page's name (printf for .../printf.txt.gz), so :man fprintf and
// :man printf share one copy and one buffer. malloc'd.
char* man_temp_path(const char* temp_dir, const char* page);

// The first DIR/whatis of `manpath` that exists, malloc'd, or NULL.
char* man_find_whatis(const char* manpath);
