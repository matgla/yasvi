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
#include <sys/types.h>

// The :term panel's shell, without any terminal I/O (editor.c draws it and
// feeds it keys). YasOS has no pseudo-terminals, so the shell runs on pipes
// (run_shell_piped): the line typed in the panel goes to its stdin when
// Enter is pressed, and its stdout and stderr come back into the scrollback.
// Nothing it runs has a terminal: programs that need one (vi, curses, a
// password prompt) do not work, and a program's output may come in blocks
// (stdio buffers a pipe fully). A program reading stdin reads the lines typed.
//
// The output is cleaned for a plain text view: ANSI escape sequences are
// dropped, \r\n ends a line, a lone \r starts the line again (progress
// bars), \b takes a character back, a tab goes to the next multiple of 8, and
// other control characters are dropped. Lines longer than TERMPANEL_LINE_MAX
// are cut there into two.

#define TERMPANEL_SCROLLBACK 500  // lines kept
#define TERMPANEL_LINE_MAX 1024
#define TERMPANEL_INPUT_MAX 256
#define TERMPANEL_HISTORY 16

// What a scrollback line is, for TermPanel.on_line.
typedef enum {
  TermPanelLine_Output,   // the shell printed it
  TermPanelLine_Command,  // a line sent to the shell (without the "$ " shown)
  TermPanelLine_Exit,     // the shell exited: the text is its status
} TermPanelLineKind;

typedef struct TermPanel {
  pid_t pid;          // the shell; 0 when none runs
  int to_shell;       // its stdin (write end), -1
  int from_shell;     // its stdout and stderr (read end), -1
  int exit_status;    // of the last shell, -1 while it runs or before one ran

  char* lines[TERMPANEL_SCROLLBACK];  // a ring, oldest at `first`
  int first;
  int count;
  char partial[TERMPANEL_LINE_MAX + 1];  // the output line still arriving
  size_t partial_length;
  int escape;  // inside an escape sequence: 1 after ESC, 2 inside CSI/OSC

  char input[TERMPANEL_INPUT_MAX + 1];  // the line being typed
  size_t input_length;
  char* history[TERMPANEL_HISTORY];  // sent lines, newest last
  int history_count;
  int history_index;  // while browsing with Up/Down; == history_count otherwise

  int scroll;     // lines scrolled back from the newest
  bool visible;   // shown under the text
  bool focused;   // keys go to the panel
  int rows;       // height on screen, title row included

  // Called for every line that goes into the scrollback (the recorder,
  // record.h); NULL for none.
  void (*on_line)(void* context, TermPanelLineKind kind, const char* text);
  void* on_line_context;
} TermPanel;

void termpanel_init(TermPanel* panel);

// Starts the shell if none runs. False if it could not be started.
bool termpanel_start(TermPanel* panel);

// Whether a shell is running.
bool termpanel_running(const TermPanel* panel);

// Reads what the shell wrote (never blocks) and notices it exiting ("[shell
// exited N]" goes into the scrollback). Returns true when the scrollback or
// the partial line changed.
bool termpanel_poll(TermPanel* panel);

// Appends output bytes as if the shell had written them (termpanel_poll's
// own path; the tests call it directly).
void termpanel_feed(TermPanel* panel, const char* data, size_t length);

// Sends `line` and a newline to the shell, echoing "$ line" into the
// scrollback and keeping it in the history. False when no shell runs or the
// pipe is full.
bool termpanel_send_line(TermPanel* panel, const char* line);

// Sends the typed line (termpanel_send_line) and clears it.
bool termpanel_submit(TermPanel* panel);

// Editing the typed line.
void termpanel_input_char(TermPanel* panel, char c);
void termpanel_input_backspace(TermPanel* panel);
void termpanel_input_clear(TermPanel* panel);
void termpanel_history_step(TermPanel* panel, int step);  // -1 older, +1 newer

// SIGINT to every process below the shell (the shell itself keeps
// running), found through /proc/PID/status PPid lines. Returns how many were
// signalled.
int termpanel_interrupt(TermPanel* panel);

// Kills the shell and everything below it, closes the pipes and reaps it.
void termpanel_stop(TermPanel* panel);

// termpanel_stop, then frees the scrollback and history.
void termpanel_deinit(TermPanel* panel);

// The scrollback: line 0 is the oldest kept.
int termpanel_line_count(const TermPanel* panel);
const char* termpanel_line(const TermPanel* panel, int index);

// The process tree below `root` from /proc: pids of its descendants into
// `out` (at most `max`). Returns how many. Exposed for the tests.
int termpanel_descendants(pid_t root, pid_t* out, int max);
