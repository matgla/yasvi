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
#include <stdio.h>
#include <time.h>

// Typing recorder (the `record` setting, config.h): every key typed while a
// file is the current buffer is appended to FILE.rec beside the file, with
// the date and time of the session, when in it the key came and the pause
// before it. Appended, never rewritten, so the file keeps the history of
// every session. Its lines (a new file starts with # lines saying this):
//
//   session DATE TIME unix SECONDS lines N
//       a session starts (local DATE TIME, the same as SECONDS since the
//       epoch); the file had N lines
//   pause DATE TIME after SECONDS s
//       the next key came after a pause of RECORD_PAUSE_MS or more, at DATE
//       TIME (SECONDS with milliseconds, e.g. 12.345)
//   AT_MS PAUSE_MS CODE KEY
//       a key: AT_MS milliseconds since the session started, PAUSE_MS since
//       the key before (0 for a session's first), CODE as getch returned
//       it, KEY its name
//
//   term AT_MS focus / text AT_MS focus
//       the keys after it go to the :term panel / back to the text
//   term AT_MS $ LINE
//       LINE went to the panel's shell (Enter in the panel, or :term CMD)
//   out AT_MS LINE
//       the shell printed LINE (as the panel shows it: escapes, \r and
//       control characters cleaned, termpanel.h)
//   term AT_MS exit N
//       the shell exited with status N
//
// Lines that start with a word are never keys; AT_MS of a term/text/out line
// counts from the start of the session that is open (keys may belong to
// another buffer's file: the panel's lines go to the recording open when
// they come, else to the current file's).
//
// KEY is the character itself (a..z, {, ...), SPACE, TAB, ENTER (^J), ESC,
// BS (^?), ^X for other control keys, UP DOWN LEFT RIGHT HOME END PGUP
// PGDN DEL KBS (curses' KEY_BACKSPACE) for curses keys (their CODE depends
// on the curses library), else K<code>.
//
// One file is open at a time: a key for another file closes it, and that
// file's next key starts a new session. Keys are buffered; recorder_idle
// writes them out once typing pauses RECORD_FLUSH_MS.

#define RECORD_SUFFIX ".rec"
#define RECORD_FLUSH_MS 1000
#define RECORD_PAUSE_MS 2000  // a pause line before the key after it

typedef struct Recorder {
  FILE* file;
  char* source;        // the file the open recording (or failure) is for
  bool failed;         // source.rec could not be opened: no retry for it
  bool pending;        // keys not yet flushed
  long long last_ms;   // monotonic time of the previous key
  long long start_ms;  // monotonic time the session started
  time_t start_time;   // wall clock time the session started
} Recorder;

void recorder_init(Recorder* recorder);

// Whether keys typed in `filename` are recorded: a named file that is not a
// recording itself.
bool record_wanted(const char* filename);

// FILE.rec, malloc'd.
char* record_path_for(const char* filename);

// The name of `key` for the recording (see above) into `out`. `curses_name`
// is the name editor.c found for a curses key, or NULL.
void record_key_name(int key, const char* curses_name, char* out, size_t size);

// `when` as local "YYYY-MM-DD HH:MM:SS" into `out`.
void record_format_time(time_t when, char* out, size_t size);

// Appends `key` (named `name`) at `now_ms` to the recording of `filename`,
// whose buffer has `lines` lines (for a new session's header), starting it
// if needed. False when the recording cannot be opened (reported once per
// file: later keys for it return true without writing).
bool recorder_key(Recorder* recorder,
                  const char* filename,
                  int lines,
                  int key,
                  const char* name,
                  long long now_ms);

// Appends "WORD AT_MS TEXT" (the :term panel's lines, above) to the open
// recording, else to `filename`'s (starting it), else nowhere (NULL). Does
// not count as a key for pauses. False when the recording cannot be opened.
bool recorder_note(Recorder* recorder,
                   const char* filename,
                   int lines,
                   const char* word,
                   const char* text,
                   long long now_ms);

// Flushes the keys when typing paused RECORD_FLUSH_MS at `now_ms`.
void recorder_idle(Recorder* recorder, long long now_ms);

// Writes the buffered keys out now (before vi execs or hands over the
// terminal).
void recorder_flush(Recorder* recorder);

// Flushes and closes the recording; the recorder can be used again.
void recorder_close(Recorder* recorder);
