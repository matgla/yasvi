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

#include "buffer.h"

/* Insert-mode word completion (Ctrl-N / Ctrl-P), as in vi's keyword
 * completion. The candidates for the word before the cursor are, in order:
 *
 *   1. words of the current buffer, scanning forward from the cursor and
 *      wrapping round -- the nearest following match first;
 *   2. words of the other open buffers;
 *   3. the filetype's own words (keywords, types, Zig's @builtins);
 *   4. names in the ctags "tags" file.
 *
 * Duplicates are dropped, and so is the word being completed. This file holds
 * no editor or ncurses state, so all of it runs in the host tests. */

#define COMPLETION_MAX_ITEMS 256

typedef struct {
  char** items;
  int count;
} CompletionList;

typedef struct {
  const Buffer* current;       // buffer being edited; its current_row is the cursor row
  const Buffer* const* others;  // other open buffers (may include `current`, skipped)
  size_t number_of_others;
  const char* tags_path;        // tags file, or NULL
} CompletionSources;

/* The column where the word ending at `cursor` starts, in `line` of `len`.
 * Identifier characters are [A-Za-z0-9_]; one of `prefix_chars` (such as
 * Zig's '@') may lead the word. Returns `cursor` when no word ends there. */
int completion_word_start(const char* line, int len, int cursor, const char* prefix_chars);

/* Fills `out` with the candidates for `prefix`, the word that starts at
 * column `start` of the current row. `out` must be empty or freed. */
void completion_collect(CompletionList* out,
                        const char* prefix,
                        int start,
                        const CompletionSources* sources);

void completion_list_free(CompletionList* list);

/* An in-progress completion: which text of which row it replaces, what the
 * user typed, and which candidate is in the row now. */
typedef struct Completion {
  bool active;
  BufferRow* row;
  int start;          // column the completed word starts at
  char* original;     // the prefix as typed
  int inserted_len;   // length of the text now at `start`
  CompletionList list;
  int selected;       // -1: the original prefix; else an index into list
} Completion;

/* Moves the selection `step` (+1 Ctrl-N, -1 Ctrl-P) through the candidates
 * and back round through the original prefix, and returns the text that
 * belongs in the row now. */
const char* completion_step(Completion* completion, int step);

/* The text for the current selection. */
const char* completion_current_text(const Completion* completion);

/* Ends the completion and frees what it holds. */
void completion_reset(Completion* completion);
