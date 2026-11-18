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

// Running other programs from vi (:make, :!cmd, :r !cmd, zcat for :man). YasOS has vfork()
// and no fork(), and its exec has the constraints apps/prun found: the argv
// and environment handed to execve live in static storage (malloc'd memory is
// refused by the kernel's uaccess check), `environ` itself is never passed (a
// copy is), and the vforked child does nothing but redirect, exec and _exit.
//
// vi waits for the child in waitpid, never reading it through a pipe: a
// process blocked in waitpid is what the kernel writes out to swap when the
// child needs the RAM. The one exception is the :term shell
// (run_shell_piped), which vi reads while it keeps running -- so vi cannot be
// swapped out for what runs there; :make and :! are the ways to run a build
// that needs vi's RAM.

// Runs MAKE_SHELL -c `script` with the terminal as its stdin/stdout and waits.
// Returns its exit status (128 + signal when killed), or -1 if it could not
// be started.
int run_shell_wait(const char* script);

// Replaces vi with MAKE_SHELL -c `script`. Returns only when the exec failed.
void run_shell_exec(const char* script);

// Runs `program` (an absolute path) with the one argument `arg`, its stdout
// and stderr written to `out_path` (created or truncated), and waits. Returns
// the exit status, or -1 if it could not be started.
int run_to_file(const char* program, const char* arg, const char* out_path);

// Runs MAKE_SHELL -c `script` with stdin from /dev/null and stdout and stderr
// written to `out_path` (created or truncated), and waits (:r !cmd). Returns
// the exit status, or -1 if it could not be started.
int run_shell_to_file(const char* script, const char* out_path);

// Starts MAKE_SHELL reading its commands from a pipe (the :term panel):
// *to_shell is the write end of its stdin, *from_shell the read end of its
// stdout and stderr. vi's two ends are non-blocking and close-on-exec. The
// shell's environment has TERM=dumb (nothing it runs has a terminal). Returns
// the shell's pid, or -1.
int run_shell_piped(int* to_shell, int* from_shell);

// Where :r !cmd's output goes before it is read into the buffer.
#ifndef RUN_OUTPUT_PATH  // the tests point it into their build directory
#define RUN_OUTPUT_PATH "/tmp/yasvi-cmd.out"
#endif

// `command` for :!cmd and :r !cmd with each % replaced by `filename` quoted
// for the shell and each \% by a plain %, as vim does. malloc'd; NULL when
// out of memory or when there is a % and `filename` is NULL (*no_name is set
// then; no_name may be NULL).
char* run_expand_command(const char* command, const char* filename, bool* no_name);

// Reads the terminal (cooked, after endwin) until Enter or end of input.
void run_wait_for_enter(void);
