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

// :make -- everything about it that does no terminal I/O: the makeprg /
// makemode settings (.yasvirc and :set), which command runs, the shell script
// that runs it, the error list read back from its log, and the session file
// that carries vi across makemode=exec. editor.c runs it (run.c spawns).

#define MAKE_SHELL "/usr/bin/bash"
#ifndef MAKE_LOG_PATH  // the tests point these into their build directory
#define MAKE_LOG_PATH "/tmp/yasvi-make.log"
#endif
#define MAKE_RC_SUFFIX ".rc"  // MAKE_LOG_PATH ".rc" holds the exit status
#ifndef MAKE_SESSION_PATH
#define MAKE_SESSION_PATH "/tmp/yasvi-session"
#endif
#define MAKE_RC_NAME ".yasvirc"

typedef enum {
  MakeMode_Wait,  // vi waits in waitpid (the kernel can swap it out)
  MakeMode_Exec,  // vi execs the build and comes back with -S
} MakeMode;

typedef struct {
  char* makeprg;  // NULL: make_resolve_program picks build.sh or make
  MakeMode mode;
} MakeConfig;

typedef enum {
  MakeSet_Ok,
  MakeSet_Ignored,     // blank or a # comment
  MakeSet_Query,       // "makeprg" without '=': show the value
  MakeSet_UnknownKey,  // not makeprg or makemode
  MakeSet_BadValue,    // makemode other than wait/exec
  MakeSet_NoMemory,
} MakeSetResult;

void make_config_init(MakeConfig* config);
void make_config_deinit(MakeConfig* config);

// One setting: "key=value" or "set key=value" (an rc line or :set's
// argument). The value is the rest of the line, spaces included; an empty
// makeprg goes back to the default.
MakeSetResult make_config_set(MakeConfig* config, const char* line);

// Every line of the rc file at `path`. False if it cannot be read; otherwise
// *bad_lines (may be NULL) counts lines that were not settings.
bool make_config_load_file(MakeConfig* config, const char* path, int* bad_lines);

// MAKE_RC_NAME in `dir` (NULL = the working directory), else in `home` (may
// be NULL). Returns the malloc'd path of the file read, or NULL if neither.
char* make_config_load_default(MakeConfig* config,
                               const char* dir,
                               const char* home,
                               int* bad_lines);

// "makeprg=... makemode=wait" into `out`; an unset makeprg shows what it
// resolves to in `dir` (NULL = working directory).
void make_config_describe(const MakeConfig* config,
                          const char* dir,
                          char* out,
                          size_t size);

// The command :make runs: makeprg, else MAKE_SHELL ./build.sh when `dir`
// (NULL = working directory) has build.sh, else make when it has a Makefile
// (or makefile). malloc'd, or NULL when there is nothing to run.
char* make_resolve_program(const MakeConfig* config, const char* dir);

// The bash -c script for the build:
//   : > LOG.rc; { PROGRAM ARGS; echo $? > LOG.rc; } 2>&1 | tee LOG
// (toybox sh has no pipefail: the status is saved inside the pipeline)
// `args` may be NULL or empty. malloc'd.
char* make_build_script(const char* program, const char* args, const char* log);

// The bash -c script makemode=exec runs in place of vi: the build as above,
// its status into LOG.rc, a wait for Enter, then `exec VI -S SESSION`.
char* make_build_exec_script(const char* program,
                             const char* args,
                             const char* log,
                             const char* vi_path,
                             const char* session);

// `text` in single quotes for the shell, malloc'd.
char* make_shell_quote(const char* text);

// The exit status saved in LOG.rc by the exec script, or -1.
int make_read_status(const char* log);

// One "file:line[:col]: error|warning: message" from the build log.
typedef struct {
  char* filename;
  int line;    // 1-based
  int column;  // 1-based; 0 when the compiler gave none (tcc)
  bool is_warning;
  char* message;
} MakeError;

typedef struct {
  MakeError* items;
  int count;
  int current;  // the entry :cc shows; -1 before the first :cn
  int errors;   // entries that are errors (the rest are warnings)
} MakeErrorList;

// Parses a log line. tcc prints "f.c:12: error: msg", gcc and zig
// "f.c:12:5: error: msg"; "fatal error" counts as an error. Notes and other
// lines give false.
bool make_parse_error_line(const char* line, MakeError* out);
void make_error_clear(MakeError* error);

// Replaces `list` with the errors and warnings of the log at `path`. Returns
// false if it cannot be read (the list is then empty).
bool make_parse_log(const char* path, MakeErrorList* list);
void make_error_list_clear(MakeErrorList* list);

// "f.c:12:5: error: msg" for the status bar, into `out`.
void make_error_format(const MakeError* error, char* out, size_t size);

// What makemode=exec keeps of vi while the build runs.
typedef struct {
  char* path;
  int line;    // 0-based
  int column;  // 0-based
  bool autosave_disabled;
} MakeSessionFile;

typedef struct {
  MakeSessionFile* files;
  int count;
  int current;  // index into files of the buffer that was shown
} MakeSession;

bool make_session_add(MakeSession* session,
                      const char* path,
                      int line,
                      int column,
                      bool autosave_disabled);
void make_session_clear(MakeSession* session);

// The session file: a header line, the settings ("set makeprg=..."), the
// current index and one "file LINE COL NOAUTOSAVE PATH" line per buffer.
bool make_session_write(const char* path,
                        const MakeSession* session,
                        const MakeConfig* config);
// Reads it back into `session` (cleared first) and `config` (NULL to skip).
bool make_session_read(const char* path, MakeSession* session, MakeConfig* config);

// What the editor keeps for :make between commands (Editor.make).
typedef struct MakeState {
  MakeConfig config;
  MakeErrorList errors;
  char* log_path;      // the log the errors came from, for :cl
  char* program_path;  // how vi was started, for makemode=exec to come back
} MakeState;
