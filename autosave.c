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

#include "autosave.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "buffer_row.h"

static char* autosave_concat(const char* a, const char* b, const char* c) {
  size_t la = strlen(a);
  size_t lb = strlen(b);
  size_t lc = strlen(c);
  char* out = malloc(la + lb + lc + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, a, la);
  memcpy(out + la, b, lb);
  memcpy(out + la + lb, c, lc + 1);
  return out;
}

long long autosave_now_ms(void) {
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return 0;
  }
  return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

char* autosave_cache_dir(void) {
  const char* xdg = getenv("XDG_CACHE_HOME");
  if (xdg != NULL && xdg[0] != '\0') {
    return autosave_concat(xdg, "/yasvi", "");
  }
  const char* home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    home = "/root";  // YasOS's only account; older kernels set no HOME
  }
  return autosave_concat(home, "/.cache/yasvi", "");
}

// `filename` as an absolute path, malloc'd. Not normalised: "a/../b" stays.
static char* autosave_absolute(const char* filename) {
  if (filename[0] == '/') {
    return strdup(filename);
  }
  while (filename[0] == '.' && filename[1] == '/') {
    filename += 2;
  }
  size_t size = 128;
  for (;;) {
    char* cwd = malloc(size);
    if (cwd == NULL) {
      return NULL;
    }
    if (getcwd(cwd, size) != NULL) {
      const char* sep = (cwd[0] != '\0' && cwd[strlen(cwd) - 1] == '/') ? "" : "/";
      char* out = autosave_concat(cwd, sep, filename);
      free(cwd);
      return out;
    }
    free(cwd);
    if (errno != ERANGE || size >= 8192) {
      return NULL;
    }
    size *= 2;
  }
}

// "<dir>/%abs%path%name.yasvi-autosave" (vim's naming for its directory list)
static char* autosave_flat_name(const char* dir, const char* filename) {
  if (filename == NULL) {
    char name[48];
    snprintf(name, sizeof(name), "/noname-%d", (int)getpid());
    return autosave_concat(dir, name, AUTOSAVE_SUFFIX);
  }
  char* absolute = autosave_absolute(filename);
  if (absolute == NULL) {
    return NULL;
  }
  for (char* p = absolute; *p != '\0'; ++p) {
    if (*p == '/') {
      *p = '%';
    }
  }
  char* dir_slash = autosave_concat(dir, "/", absolute);
  free(absolute);
  if (dir_slash == NULL) {
    return NULL;
  }
  char* out = autosave_concat(dir_slash, AUTOSAVE_SUFFIX, "");
  free(dir_slash);
  return out;
}

// "<dir>/.<name>.yasvi-autosave"
static char* autosave_beside(const char* filename) {
  const char* slash = strrchr(filename, '/');
  const char* base = slash ? slash + 1 : filename;
  if (base[0] == '\0') {
    return NULL;
  }
  size_t dir_len = slash ? (size_t)(slash - filename) + 1 : 0;
  char* dir = malloc(dir_len + 2);
  if (dir == NULL) {
    return NULL;
  }
  memcpy(dir, filename, dir_len);
  dir[dir_len] = '.';
  dir[dir_len + 1] = '\0';
  char* out = autosave_concat(dir, base, AUTOSAVE_SUFFIX);
  free(dir);
  return out;
}

int autosave_candidates(const char* filename, char* out[AUTOSAVE_MAX_CANDIDATES]) {
  int count = 0;
  if (filename != NULL && filename[0] != '\0') {
    char* beside = autosave_beside(filename);
    if (beside != NULL) {
      out[count++] = beside;
    }
  } else {
    filename = NULL;
  }
  char* cache = autosave_cache_dir();
  if (cache != NULL) {
    char* path = autosave_flat_name(cache, filename);
    free(cache);
    if (path != NULL) {
      out[count++] = path;
    }
  }
  char* tmp = autosave_flat_name("/tmp/yasvi", filename);
  if (tmp != NULL) {
    out[count++] = tmp;
  }
  return count;
}

void autosave_free_candidates(char* paths[], int count) {
  for (int i = 0; i < count; ++i) {
    free(paths[i]);
    paths[i] = NULL;
  }
}

// mkdir -p of the directory part of `path`.
static void autosave_make_parent_dirs(const char* path) {
  char* copy = strdup(path);
  if (copy == NULL) {
    return;
  }
  char* last = strrchr(copy, '/');
  if (last != NULL) {
    *last = '\0';
    for (char* p = copy + 1; *p != '\0'; ++p) {
      if (*p == '/') {
        *p = '\0';
        mkdir(copy, 0700);
        *p = '/';
      }
    }
    mkdir(copy, 0700);
  }
  free(copy);
}

bool autosave_write_to(const Buffer* buffer, const char* path) {
  char* tmp = autosave_concat(path, ".tmp", "");
  if (tmp == NULL) {
    return false;
  }
  FILE* file = fopen(tmp, "w");
  if (file == NULL) {
    free(tmp);
    return false;
  }
  bool ok = true;
  for (const BufferRow* row = buffer->head; row != NULL && ok; row = row->next) {
    if (row->data != NULL && fputs(row->data, file) == EOF) {
      ok = false;
    }
    if (ok && fputc('\n', file) == EOF) {
      ok = false;
    }
  }
  // fsync: a kernel crash must not take the copy with it, and YasOS's ext4
  // holds written blocks in its cache until then.
  if (ok && (fflush(file) != 0 || fsync(fileno(file)) != 0)) {
    ok = false;
  }
  if (fclose(file) != 0) {
    ok = false;
  }
  if (ok && rename(tmp, path) != 0) {
    // A filesystem whose rename does not replace: drop the old copy first.
    remove(path);
    ok = rename(tmp, path) == 0;
  }
  if (!ok) {
    remove(tmp);
  }
  free(tmp);
  return ok;
}

bool autosave_write(Buffer* buffer) {
  if (buffer == NULL || buffer->autosave_disabled) {
    return false;
  }
  if (buffer->autosave_path != NULL) {
    if (autosave_write_to(buffer, buffer->autosave_path)) {
      buffer->autosaved_changes = buffer->changes;
      return true;
    }
    // Its directory went away or read-only: look again from the top.
    free(buffer->autosave_path);
    buffer->autosave_path = NULL;
  }
  char* candidates[AUTOSAVE_MAX_CANDIDATES];
  int count = autosave_candidates(buffer->filename, candidates);
  bool beside = buffer->filename != NULL && buffer->filename[0] != '\0';
  for (int i = 0; i < count; ++i) {
    if (i > 0 || !beside) {
      autosave_make_parent_dirs(candidates[i]);
    }
    if (autosave_write_to(buffer, candidates[i])) {
      buffer->autosave_path = candidates[i];
      candidates[i] = NULL;
      buffer->autosaved_changes = buffer->changes;
      break;
    }
  }
  autosave_free_candidates(candidates, count);
  return buffer->autosave_path != NULL;
}

void autosave_remove(Buffer* buffer) {
  if (buffer == NULL) {
    return;
  }
  if (buffer->autosave_path != NULL) {
    remove(buffer->autosave_path);
    free(buffer->autosave_path);
    buffer->autosave_path = NULL;
  }
  buffer->autosaved_changes = buffer->changes;
}

bool autosave_matches(const Buffer* buffer, const char* path) {
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  char* line = NULL;
  size_t size = 0;
  bool same = true;
  for (const BufferRow* row = buffer->head; row != NULL && same; row = row->next) {
    ssize_t got = getline(&line, &size, file);
    const char* data = row->data ? row->data : "";
    size_t len = strlen(data);
    if (got < 0 || (size_t)got != len + 1 || memcmp(line, data, len) != 0 ||
        line[len] != '\n') {
      same = false;
    }
  }
  if (same && getline(&line, &size, file) >= 0) {
    same = false;  // the autosave has more rows
  }
  free(line);
  fclose(file);
  return same;
}

static bool autosave_exists(const char* path) {
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  fclose(file);
  return true;
}

bool autosave_check_stale(Buffer* buffer) {
  if (buffer == NULL || buffer->autosave_disabled || buffer->filename == NULL ||
      buffer->stale_autosave != NULL) {
    return buffer != NULL && buffer->stale_autosave != NULL;
  }
  char* candidates[AUTOSAVE_MAX_CANDIDATES];
  int count = autosave_candidates(buffer->filename, candidates);
  for (int i = 0; i < count; ++i) {
    if (!autosave_exists(candidates[i])) {
      continue;
    }
    if (autosave_matches(buffer, candidates[i])) {
      remove(candidates[i]);  // nothing in it the file does not have
      continue;
    }
    if (buffer->stale_autosave == NULL) {
      buffer->stale_autosave = candidates[i];
      candidates[i] = NULL;
    }
  }
  autosave_free_candidates(candidates, count);
  return buffer->stale_autosave != NULL;
}

static void autosave_free_rows(Buffer* buffer) {
  BufferRow* row = buffer->head;
  while (row != NULL) {
    BufferRow* next = row->next;
    free(row->data);
    free(row->highlight_data);
    free(row);
    row = next;
  }
  buffer->head = NULL;
  buffer->tail = NULL;
  buffer->current_row = NULL;
  buffer->number_of_rows = 0;
}

bool autosave_recover(Buffer* buffer) {
  if (buffer == NULL || buffer->stale_autosave == NULL ||
      !autosave_exists(buffer->stale_autosave)) {
    return false;
  }
  Buffer* loaded = buffer_alloc();
  if (loaded == NULL) {
    return false;
  }
  buffer_load_from_file(loaded, buffer->stale_autosave);
  if (loaded->head == NULL) {
    buffer_free(loaded);
    return false;
  }
  autosave_free_rows(buffer);
  buffer->head = loaded->head;
  buffer->tail = loaded->tail;
  buffer->current_row = loaded->head;
  buffer->number_of_rows = loaded->number_of_rows;
  loaded->head = NULL;
  loaded->tail = NULL;
  loaded->current_row = NULL;
  buffer_free(loaded);

  // Highlight as the file it is, not as "*.yasvi-autosave".
  buffer->head->highlight_comment_open = 0;
  buffer->head->highlight_string_open = 0;
  buffer->head->highlight_lang_state = 0;
  for (BufferRow* row = buffer->head; row != NULL; row = row->next) {
    row->filetype = buffer->filetype;
    row->dirty = true;
    buffer_row_highlight_line(row);
  }

  // The recovered text lives in that file already: it is now the autosave.
  autosave_remove(buffer);
  buffer->autosave_path = buffer->stale_autosave;
  buffer->stale_autosave = NULL;
  buffer_note_change(buffer);
  buffer->autosaved_changes = buffer->changes;
  buffer->observed_changes = buffer->changes;
  return true;
}

bool autosave_discard(Buffer* buffer) {
  if (buffer == NULL || buffer->stale_autosave == NULL) {
    return false;
  }
  remove(buffer->stale_autosave);
  free(buffer->stale_autosave);
  buffer->stale_autosave = NULL;
  return true;
}
