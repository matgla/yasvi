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

#define _GNU_SOURCE

#include "make.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAKE_SESSION_HEADER "yasvi-session 1"

static char* make_strndup(const char* text, size_t length) {
  char* out = malloc(length + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, text, length);
  out[length] = '\0';
  return out;
}

// The pieces concatenated, malloc'd; the list ends with NULL.
static char* make_concat(const char* first, ...) {
  size_t length = 0;
  va_list args;
  va_start(args, first);
  for (const char* piece = first; piece != NULL; piece = va_arg(args, const char*)) {
    length += strlen(piece);
  }
  va_end(args);
  char* out = malloc(length + 1);
  if (out == NULL) {
    return NULL;
  }
  char* p = out;
  va_start(args, first);
  for (const char* piece = first; piece != NULL; piece = va_arg(args, const char*)) {
    const size_t piece_length = strlen(piece);
    memcpy(p, piece, piece_length);
    p += piece_length;
  }
  va_end(args);
  *p = '\0';
  return out;
}

static bool make_file_exists(const char* dir, const char* name) {
  char* path =
    dir != NULL ? make_concat(dir, "/", name, NULL) : make_concat(name, NULL);
  if (path == NULL) {
    return false;
  }
  FILE* file = fopen(path, "r");
  free(path);
  if (file == NULL) {
    return false;
  }
  fclose(file);
  return true;
}

static const char* make_skip_spaces(const char* p) {
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  return p;
}

// Length of `text` without trailing whitespace and line ends.
static size_t make_trimmed_length(const char* text) {
  size_t length = strlen(text);
  while (length > 0 && isspace((unsigned char)text[length - 1])) {
    --length;
  }
  return length;
}

void make_config_init(MakeConfig* config) {
  config->makeprg = NULL;
  config->mode = MakeMode_Wait;
}

void make_config_deinit(MakeConfig* config) {
  free(config->makeprg);
  config->makeprg = NULL;
}

MakeSetResult make_config_set(MakeConfig* config, const char* line) {
  const char* p = make_skip_spaces(line);
  if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#') {
    return MakeSet_Ignored;
  }
  if (strncmp(p, "set", 3) == 0 && (p[3] == ' ' || p[3] == '\t')) {
    p = make_skip_spaces(p + 3);
  }
  const char* key = p;
  while (*p != '\0' && *p != '=' && !isspace((unsigned char)*p)) {
    ++p;
  }
  const size_t key_length = (size_t)(p - key);
  const bool is_makeprg = key_length == 7 && strncmp(key, "makeprg", 7) == 0;
  const bool is_makemode = key_length == 8 && strncmp(key, "makemode", 8) == 0;
  p = make_skip_spaces(p);
  if (!is_makeprg && !is_makemode) {
    return MakeSet_UnknownKey;
  }
  if (*p != '=') {
    return make_trimmed_length(p) == 0 ? MakeSet_Query : MakeSet_UnknownKey;
  }
  const char* value = make_skip_spaces(p + 1);
  const size_t value_length = make_trimmed_length(value);
  if (is_makeprg) {
    char* copy = NULL;
    if (value_length > 0) {
      copy = make_strndup(value, value_length);
      if (copy == NULL) {
        return MakeSet_NoMemory;
      }
    }
    free(config->makeprg);
    config->makeprg = copy;
    return MakeSet_Ok;
  }
  if (value_length == 4 && strncmp(value, "wait", 4) == 0) {
    config->mode = MakeMode_Wait;
  } else if (value_length == 4 && strncmp(value, "exec", 4) == 0) {
    config->mode = MakeMode_Exec;
  } else {
    return MakeSet_BadValue;
  }
  return MakeSet_Ok;
}

bool make_config_load_file(MakeConfig* config, const char* path, int* bad_lines) {
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  int bad = 0;
  char* line = NULL;
  size_t capacity = 0;
  while (getline(&line, &capacity, file) != -1) {
    const MakeSetResult result = make_config_set(config, line);
    if (result != MakeSet_Ok && result != MakeSet_Ignored) {
      ++bad;
    }
  }
  free(line);
  fclose(file);
  if (bad_lines != NULL) {
    *bad_lines = bad;
  }
  return true;
}

char* make_config_load_default(MakeConfig* config,
                               const char* dir,
                               const char* home,
                               int* bad_lines) {
  char* path = dir != NULL ? make_concat(dir, "/", MAKE_RC_NAME, NULL)
                           : make_concat(MAKE_RC_NAME, NULL);
  if (path != NULL && make_config_load_file(config, path, bad_lines)) {
    return path;
  }
  free(path);
  if (home == NULL || home[0] == '\0') {
    return NULL;
  }
  path = make_concat(home, "/", MAKE_RC_NAME, NULL);
  if (path != NULL && make_config_load_file(config, path, bad_lines)) {
    return path;
  }
  free(path);
  return NULL;
}

void make_config_describe(const MakeConfig* config,
                          const char* dir,
                          char* out,
                          size_t size) {
  const char* mode = config->mode == MakeMode_Exec ? "exec" : "wait";
  if (config->makeprg != NULL) {
    snprintf(out, size, "makeprg=%s makemode=%s", config->makeprg, mode);
    return;
  }
  char* program = make_resolve_program(config, dir);
  snprintf(out, size, "makeprg= (default: %s) makemode=%s",
           program != NULL ? program : "none, no build.sh or Makefile", mode);
  free(program);
}

char* make_resolve_program(const MakeConfig* config, const char* dir) {
  if (config->makeprg != NULL && config->makeprg[0] != '\0') {
    return make_concat(config->makeprg, NULL);
  }
  if (make_file_exists(dir, "build.sh")) {
    return make_concat(MAKE_SHELL, " ./build.sh", NULL);
  }
  if (make_file_exists(dir, "Makefile") || make_file_exists(dir, "makefile")) {
    return make_concat("make", NULL);
  }
  return NULL;
}

char* make_shell_quote(const char* text) {
  size_t length = 2;
  for (const char* p = text; *p != '\0'; ++p) {
    length += *p == '\'' ? 4 : 1;
  }
  char* out = malloc(length + 1);
  if (out == NULL) {
    return NULL;
  }
  char* q = out;
  *q++ = '\'';
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p == '\'') {
      memcpy(q, "'\\''", 4);  // close, escaped quote, reopen
      q += 4;
    } else {
      *q++ = *p;
    }
  }
  *q++ = '\'';
  *q = '\0';
  return out;
}

// "PROGRAM ARGS" with the args' surrounding spaces dropped, malloc'd.
static char* make_command_line(const char* program, const char* args) {
  if (args != NULL) {
    args = make_skip_spaces(args);
  }
  if (args == NULL || make_trimmed_length(args) == 0) {
    return make_concat(program, NULL);
  }
  char* trimmed = make_strndup(args, make_trimmed_length(args));
  if (trimmed == NULL) {
    return NULL;
  }
  char* out = make_concat(program, " ", trimmed, NULL);
  free(trimmed);
  return out;
}

// toybox sh (the board's bash) has no pipefail -- "set: bad -o pipefail" --
// so the build's own status is saved from inside the pipeline, by a brace
// group, into LOG.rc; `: >` empties it first so a stale status never shows.
char* make_build_script(const char* program, const char* args, const char* log) {
  char* command = make_command_line(program, args);
  char* quoted_log = make_shell_quote(log);
  char* rc_path = make_concat(log, MAKE_RC_SUFFIX, NULL);
  char* quoted_rc = rc_path != NULL ? make_shell_quote(rc_path) : NULL;
  char* out = NULL;
  if (command != NULL && quoted_log != NULL && quoted_rc != NULL) {
    out = make_concat(": > ", quoted_rc, "; { ", command, "; echo $? > ", quoted_rc,
                      "; } 2>&1 | tee ", quoted_log, NULL);
  }
  free(command);
  free(quoted_log);
  free(rc_path);
  free(quoted_rc);
  return out;
}

char* make_build_exec_script(const char* program,
                             const char* args,
                             const char* log,
                             const char* vi_path,
                             const char* session) {
  char* build = make_build_script(program, args, log);
  char* rc_path = make_concat(log, MAKE_RC_SUFFIX, NULL);
  char* quoted_rc = rc_path != NULL ? make_shell_quote(rc_path) : NULL;
  char* quoted_vi = make_shell_quote(vi_path);
  char* quoted_session = make_shell_quote(session);
  char* out = NULL;
  // Builtins only after the build (no $(...)): nothing more to spawn. The
  // build script already saved the status in LOG.rc; read it back.
  if (build != NULL && quoted_rc != NULL && quoted_vi != NULL &&
      quoted_session != NULL) {
    out = make_concat(build, "; read -r rc < ", quoted_rc,
                      "; echo; echo \"[build exited $rc] press Enter\"; read -r _; ",
                      "exec ", quoted_vi, " -S ", quoted_session, NULL);
  }
  free(build);
  free(rc_path);
  free(quoted_rc);
  free(quoted_vi);
  free(quoted_session);
  return out;
}

int make_read_status(const char* log) {
  char* rc_path = make_concat(log, MAKE_RC_SUFFIX, NULL);
  if (rc_path == NULL) {
    return -1;
  }
  FILE* file = fopen(rc_path, "r");
  free(rc_path);
  if (file == NULL) {
    return -1;
  }
  char text[16];
  int status = -1;
  if (fgets(text, sizeof(text), file) != NULL) {
    char* end = NULL;
    long value = strtol(text, &end, 10);
    if (end != text && value >= 0 && value <= 255) {
      status = (int)value;
    }
  }
  fclose(file);
  return status;
}

// Digits at *p as a positive int; *p moves past them. -1 if none.
static int make_parse_number(const char** p) {
  if (!isdigit((unsigned char)**p)) {
    return -1;
  }
  long value = 0;
  while (isdigit((unsigned char)**p)) {
    if (value < 100000000) {
      value = value * 10 + (**p - '0');
    }
    ++*p;
  }
  return (int)value;
}

bool make_parse_error_line(const char* line, MakeError* out) {
  const char* start = make_skip_spaces(line);
  // The filename ends at the first ":<digits>:" that a kind follows, so a
  // name with a colon in it still parses.
  for (const char* colon = strchr(start, ':'); colon != NULL;
       colon = strchr(colon + 1, ':')) {
    if (colon == start) {
      continue;
    }
    const char* p = colon + 1;
    const int line_number = make_parse_number(&p);
    if (line_number <= 0 || *p != ':') {
      continue;
    }
    ++p;
    int column = 0;
    const char* after_column = p;
    const int maybe_column = make_parse_number(&after_column);
    if (maybe_column > 0 && *after_column == ':') {
      column = maybe_column;
      p = after_column + 1;
    }
    p = make_skip_spaces(p);
    bool is_warning;
    if (strncmp(p, "error:", 6) == 0) {
      is_warning = false;
      p += 6;
    } else if (strncmp(p, "fatal error:", 12) == 0) {
      is_warning = false;
      p += 12;
    } else if (strncmp(p, "warning:", 8) == 0) {
      is_warning = true;
      p += 8;
    } else {
      continue;
    }
    p = make_skip_spaces(p);
    out->filename = make_strndup(start, (size_t)(colon - start));
    out->message = make_strndup(p, make_trimmed_length(p));
    if (out->filename == NULL || out->message == NULL) {
      make_error_clear(out);
      return false;
    }
    out->line = line_number;
    out->column = column;
    out->is_warning = is_warning;
    return true;
  }
  return false;
}

void make_error_clear(MakeError* error) {
  free(error->filename);
  free(error->message);
  error->filename = NULL;
  error->message = NULL;
}

void make_error_list_clear(MakeErrorList* list) {
  for (int i = 0; i < list->count; ++i) {
    make_error_clear(&list->items[i]);
  }
  free(list->items);
  list->items = NULL;
  list->count = 0;
  list->errors = 0;
  list->current = -1;
}

bool make_parse_log(const char* path, MakeErrorList* list) {
  make_error_list_clear(list);
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  int capacity = 0;
  char* line = NULL;
  size_t line_capacity = 0;
  while (getline(&line, &line_capacity, file) != -1) {
    MakeError error;
    if (!make_parse_error_line(line, &error)) {
      continue;
    }
    if (list->count == capacity) {
      const int grown = capacity == 0 ? 8 : capacity * 2;
      MakeError* items = realloc(list->items, sizeof(MakeError) * (size_t)grown);
      if (items == NULL) {
        make_error_clear(&error);
        break;  // keep what fits
      }
      list->items = items;
      capacity = grown;
    }
    list->items[list->count++] = error;
    if (!error.is_warning) {
      ++list->errors;
    }
  }
  free(line);
  fclose(file);
  return true;
}

void make_error_format(const MakeError* error, char* out, size_t size) {
  const char* kind = error->is_warning ? "warning" : "error";
  if (error->column > 0) {
    snprintf(out, size, "%s:%d:%d: %s: %s", error->filename, error->line,
             error->column, kind, error->message);
  } else {
    snprintf(out, size, "%s:%d: %s: %s", error->filename, error->line, kind,
             error->message);
  }
}

bool make_session_add(MakeSession* session,
                      const char* path,
                      int line,
                      int column,
                      bool autosave_disabled) {
  char* copy = make_concat(path, NULL);
  if (copy == NULL) {
    return false;
  }
  MakeSessionFile* files =
    realloc(session->files, sizeof(MakeSessionFile) * (size_t)(session->count + 1));
  if (files == NULL) {
    free(copy);
    return false;
  }
  session->files = files;
  MakeSessionFile* file = &session->files[session->count++];
  file->path = copy;
  file->line = line < 0 ? 0 : line;
  file->column = column < 0 ? 0 : column;
  file->autosave_disabled = autosave_disabled;
  return true;
}

void make_session_clear(MakeSession* session) {
  for (int i = 0; i < session->count; ++i) {
    free(session->files[i].path);
  }
  free(session->files);
  session->files = NULL;
  session->count = 0;
  session->current = 0;
}

bool make_session_write(const char* path,
                        const MakeSession* session,
                        const MakeConfig* config) {
  FILE* file = fopen(path, "w");
  if (file == NULL) {
    return false;
  }
  bool ok = fprintf(file, "%s\n", MAKE_SESSION_HEADER) >= 0;
  if (config != NULL) {
    if (config->makeprg != NULL) {
      ok = ok && fprintf(file, "set makeprg=%s\n", config->makeprg) >= 0;
    }
    ok = ok && fprintf(file, "set makemode=%s\n",
                       config->mode == MakeMode_Exec ? "exec" : "wait") >= 0;
  }
  ok = ok && fprintf(file, "current %d\n", session->current) >= 0;
  for (int i = 0; ok && i < session->count; ++i) {
    const MakeSessionFile* entry = &session->files[i];
    ok = fprintf(file, "file %d %d %d %s\n", entry->line, entry->column,
                 entry->autosave_disabled ? 1 : 0, entry->path) >= 0;
  }
  if (fclose(file) != 0) {
    ok = false;
  }
  return ok;
}

// "file LINE COL NOAUTOSAVE PATH"
static bool make_session_parse_file(MakeSession* session, const char* line) {
  const char* p = line + 5;
  int numbers[3];
  for (int i = 0; i < 3; ++i) {
    p = make_skip_spaces(p);
    numbers[i] = make_parse_number(&p);
    if (numbers[i] < 0 || *p != ' ') {
      return false;
    }
  }
  const char* path = p + 1;
  const size_t length = strlen(path);
  size_t path_length = length;
  while (path_length > 0 &&
         (path[path_length - 1] == '\n' || path[path_length - 1] == '\r')) {
    --path_length;
  }
  if (path_length == 0) {
    return false;
  }
  char* copy = make_strndup(path, path_length);
  if (copy == NULL) {
    return false;
  }
  const bool added =
    make_session_add(session, copy, numbers[0], numbers[1], numbers[2] != 0);
  free(copy);
  return added;
}

bool make_session_read(const char* path, MakeSession* session, MakeConfig* config) {
  make_session_clear(session);
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  char* line = NULL;
  size_t capacity = 0;
  bool ok = getline(&line, &capacity, file) != -1 &&
            strncmp(line, MAKE_SESSION_HEADER, strlen(MAKE_SESSION_HEADER)) == 0;
  while (ok && getline(&line, &capacity, file) != -1) {
    if (strncmp(line, "set ", 4) == 0) {
      if (config != NULL) {
        make_config_set(config, line);
      }
    } else if (strncmp(line, "current ", 8) == 0) {
      const char* p = line + 8;
      const int current = make_parse_number(&p);
      session->current = current < 0 ? 0 : current;
    } else if (strncmp(line, "file ", 5) == 0) {
      make_session_parse_file(session, line);  // a bad line loses one file
    }
  }
  free(line);
  fclose(file);
  if (!ok) {
    make_session_clear(session);
    return false;
  }
  if (session->current >= session->count) {
    session->current = 0;
  }
  return true;
}
