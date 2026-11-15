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

// Crash recovery: a modified buffer is copied, as plain text, to an autosave
// file once typing pauses (AUTOSAVE_IDLE_MS) or after AUTOSAVE_MAX_PENDING
// edits, whichever comes first. :w and a clean exit delete it, so one that is
// still there when the file is opened again was left by a crash.
//
// Where it goes, first that can be written:
//   1. beside the file:   <dir>/.<name>.yasvi-autosave   (like vim's .swp)
//   2. the cache dir:     <cache>/%abs%path%to%name.yasvi-autosave
//      <cache> = $XDG_CACHE_HOME/yasvi, else $HOME/.cache/yasvi, else
//      /root/.cache/yasvi -- for read-only directories (romfs, a ro mount)
//   3. /tmp/yasvi, same naming as 2 (survives the editor, not a reboot)
// An unnamed buffer only has 2 and 3, as noname-<pid>.yasvi-autosave.

#define AUTOSAVE_SUFFIX ".yasvi-autosave"
#define AUTOSAVE_IDLE_MS 2000
#define AUTOSAVE_MAX_PENDING 200
#define AUTOSAVE_MAX_CANDIDATES 3

// Monotonic milliseconds, for the idle timer.
long long autosave_now_ms(void);

// The candidate paths for `filename` (NULL = unnamed buffer), best first.
// Returns how many were stored in `out`; each is malloc'd, free with
// autosave_free_candidates.
int autosave_candidates(const char* filename, char* out[AUTOSAVE_MAX_CANDIDATES]);
void autosave_free_candidates(char* paths[], int count);

// The autosave directory used when the file's own directory is read-only.
// malloc'd; never NULL unless out of memory.
char* autosave_cache_dir(void);

// Writes the buffer's rows to `path` through a temporary file and a rename,
// so a crash while writing leaves the previous autosave whole.
bool autosave_write_to(const Buffer* buffer, const char* path);

// Writes the buffer's autosave, picking the first candidate that works the
// first time and remembering it in buffer->autosave_path.
bool autosave_write(Buffer* buffer);

// Deletes this session's autosave (not one left by a crash) and forgets it.
void autosave_remove(Buffer* buffer);

// Looks for an autosave left behind for the just-loaded `buffer`. One whose
// text equals the buffer's is deleted; another is kept in
// buffer->stale_autosave (and autosaving the buffer waits until
// autosave_recover or autosave_discard decides about it). True if one was kept.
bool autosave_check_stale(Buffer* buffer);

// Replaces the buffer's text with its stale autosave, which becomes its
// autosave from now on. The buffer is then modified, for :w to keep.
bool autosave_recover(Buffer* buffer);

// Deletes the stale autosave.
bool autosave_discard(Buffer* buffer);

// Whether `path` holds exactly the rows of `buffer`, as :w would write them.
bool autosave_matches(const Buffer* buffer, const char* path);
