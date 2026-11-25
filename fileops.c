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

#include "fileops.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILEOPS_COPY_CHUNK 4096

static char g_fileops_error[320];

static char* fileops_strdup(const char* text) {
  size_t length = strlen(text) + 1;
  char* copy = malloc(length);
  if (copy != NULL) {
    memcpy(copy, text, length);
  }
  return copy;
}

static const char* fileops_fail(const char* verb, const char* path, int error) {
  snprintf(g_fileops_error, sizeof(g_fileops_error), "Cannot %s %s: %s", verb,
           path_basename(path), strerror(error));
  return g_fileops_error;
}

static const char* fileops_say(const char* text, const char* path) {
  snprintf(g_fileops_error, sizeof(g_fileops_error), text, path_basename(path));
  return g_fileops_error;
}

/* ---- paths ---------------------------------------------------------------- */

char* path_join(const char* dir, const char* name) {
  size_t dir_length = strlen(dir);
  while (dir_length > 1 && dir[dir_length - 1] == '/') {
    --dir_length;
  }
  bool slash = !(dir_length == 1 && dir[0] == '/');
  size_t name_length = strlen(name);
  char* out = malloc(dir_length + (slash ? 1 : 0) + name_length + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, dir, dir_length);
  size_t used = dir_length;
  if (slash) {
    out[used++] = '/';
  }
  memcpy(out + used, name, name_length + 1);
  return out;
}

char* path_expand_home(const char* path) {
  if (path[0] == '~' && (path[1] == '\0' || path[1] == '/')) {
    const char* home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
      home = "/root";
    }
    if (path[1] == '\0') {
      return fileops_strdup(home);
    }
    return path_join(home, path + 2);
  }
  return fileops_strdup(path);
}

char* path_absolute(const char* path) {
  char* expanded = path_expand_home(path);
  if (expanded == NULL) {
    return NULL;
  }
  char* whole = expanded;
  if (expanded[0] != '/') {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      strcpy(cwd, "/");
    }
    whole = path_join(cwd, expanded);
    free(expanded);
    if (whole == NULL) {
      return NULL;
    }
  }
  // Rebuild from the parts: out never grows past the input.
  size_t length = strlen(whole);
  char* out = malloc(length + 2);
  if (out == NULL) {
    free(whole);
    return NULL;
  }
  size_t used = 0;
  const char* p = whole;
  while (*p != '\0') {
    while (*p == '/') {
      ++p;
    }
    const char* start = p;
    while (*p != '\0' && *p != '/') {
      ++p;
    }
    size_t part = (size_t)(p - start);
    if (part == 0 || (part == 1 && start[0] == '.')) {
      continue;
    }
    if (part == 2 && start[0] == '.' && start[1] == '.') {
      while (used > 0 && out[used - 1] != '/') {
        --used;
      }
      if (used > 0) {
        --used;  // the slash before the part taken back
      }
      continue;
    }
    out[used++] = '/';
    memcpy(out + used, start, part);
    used += part;
  }
  if (used == 0) {
    out[used++] = '/';
  }
  out[used] = '\0';
  free(whole);
  return out;
}

bool path_same(const char* a, const char* b) {
  if (a == NULL || b == NULL) {
    return false;
  }
  if (strcmp(a, b) == 0) {
    return true;
  }
  char* full_a = path_absolute(a);
  char* full_b = path_absolute(b);
  bool same = full_a != NULL && full_b != NULL && strcmp(full_a, full_b) == 0;
  free(full_a);
  free(full_b);
  return same;
}

bool path_is_within(const char* path, const char* dir) {
  char* full_path = path_absolute(path);
  char* full_dir = path_absolute(dir);
  bool within = false;
  if (full_path != NULL && full_dir != NULL) {
    size_t length = strlen(full_dir);
    if (strcmp(full_dir, "/") == 0) {
      within = true;
    } else if (strncmp(full_path, full_dir, length) == 0) {
      within = full_path[length] == '\0' || full_path[length] == '/';
    }
  }
  free(full_path);
  free(full_dir);
  return within;
}

char* path_dirname(const char* path) {
  const char* slash = strrchr(path, '/');
  if (slash == NULL) {
    return fileops_strdup(".");
  }
  if (slash == path) {
    return fileops_strdup("/");
  }
  size_t length = (size_t)(slash - path);
  char* out = malloc(length + 1);
  if (out != NULL) {
    memcpy(out, path, length);
    out[length] = '\0';
  }
  return out;
}

const char* path_basename(const char* path) {
  const char* slash = strrchr(path, '/');
  if (slash != NULL && slash[1] == '\0' && slash != path) {
    // "dir/": the name before the trailing slash, for messages
    const char* start = slash;
    while (start > path && start[-1] != '/') {
      --start;
    }
    return start;
  }
  return slash != NULL ? slash + 1 : path;
}

char* path_resolve(const char* dir, const char* name) {
  char* expanded = path_expand_home(name);
  if (expanded == NULL || expanded[0] == '/') {
    return expanded;
  }
  char* out = path_join(dir, expanded);
  free(expanded);
  return out;
}

/* ---- operations ----------------------------------------------------------- */

static bool fileops_exists(const char* path) {
  struct stat st;
  return lstat(path, &st) == 0;
}

const char* fileops_create_file(const char* path) {
  if (fileops_exists(path)) {
    return fileops_say("%s already exists", path);
  }
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd < 0) {
    return fileops_fail("create", path, errno);
  }
  close(fd);
  return NULL;
}

const char* fileops_make_directory(const char* path) {
  if (fileops_exists(path)) {
    return fileops_say("%s already exists", path);
  }
  char* parent = path_dirname(path);
  if (parent == NULL) {
    return "Out of memory";
  }
  struct stat st;
  if (strcmp(parent, path) != 0 && stat(parent, &st) != 0) {
    const char* error = fileops_make_directory(parent);
    if (error != NULL) {
      free(parent);
      return error;
    }
  }
  free(parent);
  if (mkdir((char*)path, 0755) != 0) {
    return fileops_fail("create", path, errno);
  }
  return NULL;
}

static const char* fileops_copy_file(const char* from, const char* to, int mode) {
  int in = open(from, O_RDONLY);
  if (in < 0) {
    return fileops_fail("read", from, errno);
  }
  int out = open(to, O_WRONLY | O_CREAT | O_EXCL, mode);
  if (out < 0) {
    int error = errno;
    close(in);
    return fileops_fail("create", to, error);
  }
  char* chunk = malloc(FILEOPS_COPY_CHUNK);
  const char* result = chunk == NULL ? "Out of memory" : NULL;
  while (result == NULL) {
    ssize_t got = read(in, chunk, FILEOPS_COPY_CHUNK);
    if (got < 0) {
      result = fileops_fail("read", from, errno);
      break;
    }
    if (got == 0) {
      break;
    }
    ssize_t written = 0;
    while (written < got) {
      ssize_t n = write(out, chunk + written, (size_t)(got - written));
      if (n <= 0) {
        result = fileops_fail("write", to, n < 0 ? errno : ENOSPC);
        break;
      }
      written += n;
    }
  }
  free(chunk);
  close(in);
  if (close(out) != 0 && result == NULL) {
    result = fileops_fail("write", to, errno);
  }
  if (result != NULL) {
    unlink(to);  // no half copy left behind
  }
  return result;
}

static const char* fileops_copy_tree(const char* from, const char* to) {
  struct stat st;
  if (stat(from, &st) != 0) {
    return fileops_fail("copy", from, errno);
  }
  int mode = (int)(st.st_mode & 0777);
  if (!S_ISDIR(st.st_mode)) {
    return fileops_copy_file(from, to, mode != 0 ? mode : 0644);
  }
  if (mkdir((char*)to, 0755) != 0) {
    return fileops_fail("create", to, errno);
  }
  DIR* dir = opendir(from);
  if (dir == NULL) {
    return fileops_fail("read", from, errno);
  }
  const char* result = NULL;
  struct dirent* entry;
  while (result == NULL && (entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    char* source = path_join(from, entry->d_name);
    char* target = path_join(to, entry->d_name);
    result = source == NULL || target == NULL ? "Out of memory"
                                              : fileops_copy_tree(source, target);
    free(source);
    free(target);
  }
  closedir(dir);
  return result;
}

const char* fileops_copy(const char* from, const char* to) {
  if (!fileops_exists(from)) {
    return fileops_fail("copy", from, ENOENT);
  }
  if (fileops_exists(to)) {
    return fileops_say("%s already exists", to);
  }
  if (path_is_within(to, from)) {
    return "Cannot copy a directory into itself";
  }
  return fileops_copy_tree(from, to);
}

const char* fileops_remove(const char* path) {
  struct stat st;
  if (lstat(path, &st) != 0) {
    return fileops_fail("delete", path, errno);
  }
  char* full = path_absolute(path);
  bool is_root = full != NULL && strcmp(full, "/") == 0;
  free(full);
  if (is_root) {
    return "Refusing to delete /";
  }
  if (!S_ISDIR(st.st_mode)) {
    return unlink(path) == 0 ? NULL : fileops_fail("delete", path, errno);
  }
  DIR* dir = opendir(path);
  if (dir == NULL) {
    return fileops_fail("read", path, errno);
  }
  // Names first, then deletes: removing entries while readdir walks the
  // directory is not something every file system here handles.
  char** names = NULL;
  int count = 0;
  int capacity = 0;
  const char* result = NULL;
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    if (count == capacity) {
      capacity = capacity == 0 ? 16 : capacity * 2;
      char** grown = realloc(names, sizeof(char*) * (size_t)capacity);
      if (grown == NULL) {
        result = "Out of memory";
        break;
      }
      names = grown;
    }
    names[count] = path_join(path, entry->d_name);
    if (names[count] == NULL) {
      result = "Out of memory";
      break;
    }
    ++count;
  }
  closedir(dir);
  for (int i = 0; i < count; ++i) {
    if (result == NULL) {
      result = fileops_remove(names[i]);
    }
    free(names[i]);
  }
  free(names);
  if (result == NULL && rmdir(path) != 0) {
    result = fileops_fail("delete", path, errno);
  }
  return result;
}

const char* fileops_rename(const char* from, const char* to) {
  if (!fileops_exists(from)) {
    return fileops_fail("rename", from, ENOENT);
  }
  if (path_same(from, to)) {
    return NULL;
  }
  if (fileops_exists(to)) {
    return fileops_say("%s already exists", to);
  }
  if (path_is_within(to, from)) {
    return "Cannot move a directory into itself";
  }
  if (rename(from, to) == 0) {
    return NULL;
  }
  if (errno != EXDEV) {
    return fileops_fail("rename", from, errno);
  }
  // Another file system: copy, then delete the original.
  const char* error = fileops_copy_tree(from, to);
  if (error != NULL) {
    return error;
  }
  return fileops_remove(from);
}

/* ---- completion ----------------------------------------------------------- */

static int path_compare(const void* a, const void* b) {
  return strcmp(*(char* const*)a, *(char* const*)b);
}

int path_complete(const char* partial, char*** matches) {
  return path_complete_in(NULL, partial, matches);
}

int path_complete_in(const char* base, const char* partial, char*** matches) {
  *matches = NULL;
  const char* slash = strrchr(partial, '/');
  size_t typed_dir_length = slash != NULL ? (size_t)(slash - partial) + 1 : 0;
  const char* prefix = partial + typed_dir_length;
  size_t prefix_length = strlen(prefix);

  char* dir_path;
  if (typed_dir_length == 0) {
    dir_path = fileops_strdup(base != NULL ? base : ".");
  } else if (base != NULL && partial[0] != '/' && partial[0] != '~') {
    char* typed = malloc(typed_dir_length + 1);
    if (typed == NULL) {
      return 0;
    }
    memcpy(typed, partial, typed_dir_length);
    typed[typed_dir_length] = '\0';
    dir_path = path_join(base, typed);
    free(typed);
  } else {
    char* typed = malloc(typed_dir_length + 1);
    if (typed == NULL) {
      return 0;
    }
    memcpy(typed, partial, typed_dir_length);
    typed[typed_dir_length] = '\0';
    dir_path = path_expand_home(typed);
    free(typed);
  }
  if (dir_path == NULL) {
    return 0;
  }
  DIR* dir = opendir(dir_path);
  if (dir == NULL) {
    free(dir_path);
    return 0;
  }
  char** out = NULL;
  int count = 0;
  int capacity = 0;
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    const char* name = entry->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }
    if (name[0] == '.' && prefix[0] != '.') {
      continue;
    }
    if (strncmp(name, prefix, prefix_length) != 0) {
      continue;
    }
    char* full = path_join(dir_path, name);
    struct stat st;
    bool is_dir = full != NULL && stat(full, &st) == 0 && S_ISDIR(st.st_mode);
    free(full);
    if (count == capacity) {
      capacity = capacity == 0 ? 16 : capacity * 2;
      char** grown = realloc(out, sizeof(char*) * (size_t)capacity);
      if (grown == NULL) {
        break;
      }
      out = grown;
    }
    size_t name_length = strlen(name);
    char* match = malloc(typed_dir_length + name_length + 2);
    if (match == NULL) {
      break;
    }
    memcpy(match, partial, typed_dir_length);
    memcpy(match + typed_dir_length, name, name_length);
    size_t used = typed_dir_length + name_length;
    if (is_dir) {
      match[used++] = '/';
    }
    match[used] = '\0';
    out[count++] = match;
  }
  closedir(dir);
  free(dir_path);
  if (count > 1) {
    qsort(out, (size_t)count, sizeof(char*), path_compare);
  }
  if (count == 0) {
    free(out);
    out = NULL;
  }
  *matches = out;
  return count;
}

void path_matches_free(char** matches, int count) {
  for (int i = 0; i < count; ++i) {
    free(matches[i]);
  }
  free(matches);
}

char* path_common_prefix(char** matches, int count) {
  if (count == 0) {
    return fileops_strdup("");
  }
  size_t length = strlen(matches[0]);
  for (int i = 1; i < count; ++i) {
    size_t j = 0;
    while (j < length && matches[i][j] == matches[0][j]) {
      ++j;
    }
    length = j;
  }
  char* out = malloc(length + 1);
  if (out != NULL) {
    memcpy(out, matches[0], length);
    out[length] = '\0';
  }
  return out;
}
