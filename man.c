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

#include "man.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DIR + "/" + NAME + SUFFIX, malloc'd. `dir_len` bytes of `dir` are used, so
// a directory can be taken straight out of the MANPATH string.
static char* man_join(const char* dir,
                      size_t dir_len,
                      const char* name,
                      size_t name_len,
                      const char* suffix) {
  const size_t suffix_len = strlen(suffix);
  char* out = malloc(dir_len + 1 + name_len + suffix_len + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, dir, dir_len);
  out[dir_len] = '/';
  memcpy(out + dir_len + 1, name, name_len);
  memcpy(out + dir_len + 1 + name_len, suffix, suffix_len + 1);
  return out;
}

static bool man_file_exists(const char* path) {
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  fclose(file);
  return true;
}

static const char* man_path_or_default(const char* manpath) {
  if (manpath == NULL || manpath[0] == '\0') {
    return MAN_DEFAULT_PATH;
  }
  return manpath;
}

// The next directory of a colon-separated list: its start goes to *dir and
// its length is returned; *cursor moves past it. Returns -1 at the end.
// Empty entries ("a::b") are skipped, as the shell's word splitting does.
static int man_next_dir(const char** cursor, const char** dir) {
  const char* p = *cursor;
  while (*p == ':') {
    ++p;
  }
  if (*p == '\0') {
    *cursor = p;
    return -1;
  }
  const char* end = strchr(p, ':');
  if (end == NULL) {
    end = p + strlen(p);
  }
  *dir = p;
  *cursor = end;
  return (int)(end - p);
}

static char* man_strndup(const char* text, size_t length) {
  char* out = malloc(length + 1);
  if (out != NULL) {
    memcpy(out, text, length);
    out[length] = '\0';
  }
  return out;
}

// The line "NAME BLOCK OFFSET LENGTH" of DIR/index, as a ManPage.
static bool man_in_index(const char* dir,
                         size_t dir_len,
                         const char* name,
                         size_t name_len,
                         ManPage* out) {
  char* index = man_join(dir, dir_len, "index", 5, "");
  if (index == NULL) {
    return false;
  }
  FILE* file = fopen(index, "r");
  free(index);
  if (file == NULL) {
    return false;
  }
  bool found = false;
  char* line = NULL;
  size_t capacity = 0;
  while (getline(&line, &capacity, file) != -1) {
    if (strncmp(line, name, name_len) != 0 || line[name_len] != ' ') {
      continue;
    }
    // The first matching line decides, like the script's grep -m1.
    char block[64];
    long offset = -1;
    long length = -1;
    if (sscanf(line + name_len + 1, "%63s %ld %ld", block, &offset, &length) == 3 &&
        offset >= 0 && length >= 0) {
      out->file = man_join(dir, dir_len, block, strlen(block), "");
      out->name = man_strndup(name, name_len);
      out->offset = offset;
      out->length = length;
      found = out->file != NULL && out->name != NULL;
      if (!found) {
        man_page_free(out);
      }
    }
    break;
  }
  free(line);
  fclose(file);
  return found;
}

// The page NAME in one directory, the script's in_dir: DIR/NAME.txt, else
// DIR/NAME.txt.gz, else its line in DIR/index. `dir_len` bytes of `dir` are
// used, so a directory can be taken straight out of the MANPATH string.
static bool man_in_dir(const char* dir,
                       size_t dir_len,
                       const char* name,
                       size_t name_len,
                       ManPage* out) {
  static const char* const suffixes[] = {".txt", ".txt.gz"};
  for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
    char* path = man_join(dir, dir_len, name, name_len, suffixes[i]);
    if (path == NULL) {
      return false;
    }
    if (man_file_exists(path)) {
      out->file = path;
      out->name = man_strndup(name, name_len);
      out->offset = -1;
      out->length = -1;
      if (out->name == NULL) {
        man_page_free(out);
        return false;
      }
      return true;
    }
    free(path);
  }
  return man_in_index(dir, dir_len, name, name_len, out);
}

// The page for the first line of DIR/aliases naming `name`, or NULL.
static bool man_find_alias(const char* dir,
                           size_t dir_len,
                           const char* name,
                           ManPage* out) {
  char* aliases = man_join(dir, dir_len, "aliases", 7, "");
  if (aliases == NULL) {
    return false;
  }
  FILE* file = fopen(aliases, "r");
  free(aliases);
  if (file == NULL) {
    return false;
  }
  const size_t name_len = strlen(name);
  bool result = false;
  char* line = NULL;
  size_t capacity = 0;
  while (getline(&line, &capacity, file) != -1) {
    if (strncmp(line, name, name_len) != 0 || line[name_len] != ' ') {
      continue;
    }
    // Like the script's "${alias#* }": the page is what follows the first
    // space; the first matching line decides (grep -m1).
    char* page = line + name_len + 1;
    size_t page_len = strlen(page);
    while (page_len > 0 &&
           (page[page_len - 1] == '\n' || page[page_len - 1] == '\r' ||
            page[page_len - 1] == ' ')) {
      --page_len;
    }
    if (page_len > 0) {
      result = man_in_dir(dir, dir_len, page, page_len, out);
    }
    break;
  }
  free(line);
  fclose(file);
  return result;
}

bool man_locate(const char* manpath, const char* name, ManPage* out) {
  out->file = NULL;
  out->name = NULL;
  out->offset = -1;
  out->length = -1;
  if (name == NULL || name[0] == '\0') {
    return false;
  }
  const char* list = man_path_or_default(manpath);
  const size_t name_len = strlen(name);
  const char* cursor = list;
  const char* dir = NULL;
  int dir_len;
  while ((dir_len = man_next_dir(&cursor, &dir)) >= 0) {
    if (man_in_dir(dir, (size_t)dir_len, name, name_len, out)) {
      return true;
    }
  }
  cursor = list;
  while ((dir_len = man_next_dir(&cursor, &dir)) >= 0) {
    if (man_find_alias(dir, (size_t)dir_len, name, out)) {
      return true;
    }
  }
  return false;
}

void man_page_free(ManPage* page) {
  free(page->file);
  free(page->name);
  page->file = NULL;
  page->name = NULL;
}

char* man_find_page(const char* manpath, const char* name) {
  ManPage page;
  if (!man_locate(manpath, name, &page)) {
    return NULL;
  }
  char* file = page.file;
  page.file = NULL;
  man_page_free(&page);
  return file;
}

bool man_cut(const char* path, long offset, long length) {
  if (offset < 0 || length < 0) {
    return false;
  }
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  char* data = malloc((size_t)length + 1);
  bool ok = data != NULL && fseek(file, offset, SEEK_SET) == 0 &&
            fread(data, 1, (size_t)length, file) == (size_t)length;
  fclose(file);
  if (ok) {
    file = fopen(path, "wb");
    ok = file != NULL && fwrite(data, 1, (size_t)length, file) == (size_t)length;
    if (file != NULL && fclose(file) != 0) {
      ok = false;
    }
  }
  free(data);
  return ok;
}

char* man_find_whatis(const char* manpath) {
  const char* cursor = man_path_or_default(manpath);
  const char* dir = NULL;
  int dir_len;
  while ((dir_len = man_next_dir(&cursor, &dir)) >= 0) {
    char* path = man_join(dir, (size_t)dir_len, "whatis", 6, "");
    if (path == NULL) {
      return NULL;
    }
    if (man_file_exists(path)) {
      return path;
    }
    free(path);
  }
  return NULL;
}

bool man_is_gzipped(const char* path) {
  const size_t length = strlen(path);
  return length > 3 && strcmp(path + length - 3, ".gz") == 0;
}

char* man_temp_path(const char* temp_dir, const char* page) {
  const char* base = strrchr(page, '/');
  base = base != NULL ? base + 1 : page;
  size_t base_len = strlen(base);
  if (base_len > 3 && strcmp(base + base_len - 3, ".gz") == 0) {
    base_len -= 3;
  }
  if (base_len > 4 && strncmp(base + base_len - 4, ".txt", 4) == 0) {
    base_len -= 4;
  }
  const size_t dir_len = strlen(temp_dir);
  const size_t prefix_len = strlen(MAN_TEMP_PREFIX);
  char* out = malloc(dir_len + 1 + prefix_len + base_len + 5);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, temp_dir, dir_len);
  out[dir_len] = '/';
  memcpy(out + dir_len + 1, MAN_TEMP_PREFIX, prefix_len);
  memcpy(out + dir_len + 1 + prefix_len, base, base_len);
  memcpy(out + dir_len + 1 + prefix_len + base_len, ".txt", 5);
  return out;
}
