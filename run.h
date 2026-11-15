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

// Running other programs from vi (:make, zcat for :man). YasOS has vfork()
// and no fork(), and its exec has the constraints apps/prun found: the argv
// and environment handed to execve live in static storage (malloc'd memory is
// refused by the kernel's uaccess check), `environ` itself is never passed (a
// copy is), and the vforked child does nothing but redirect, exec and _exit.
//
// vi waits for the child in waitpid, never reading it through a pipe: a
// process blocked in waitpid is what the kernel writes out to swap when the
// child needs the RAM.

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

// Reads the terminal (cooked, after endwin) until Enter or end of input.
void run_wait_for_enter(void);
