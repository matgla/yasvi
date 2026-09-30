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

#include "tags.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* tags_strndup(const char* s, size_t n) {
  char* copy = (char*)malloc(n + 1);
  if (copy == NULL) {
    return NULL;
  }
  memcpy(copy, s, n);
  copy[n] = '\0';
  return copy;
}

static void tags_clear_entry(TagEntry* entry) {
  free(entry->name);
  free(entry->filename);
  free(entry->pattern);
  memset(entry, 0, sizeof(*entry));
}

static bool tags_file_readable(const char* path) {
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  fclose(file);
  return true;
}

/* "dir/tags", or plain "tags" for an empty dir. */
static char* tags_path_in(const char* dir, size_t dir_len) {
  static const char name[] = "tags";
  char* path = (char*)malloc(dir_len + sizeof(name) + 1);
  if (path == NULL) {
    return NULL;
  }
  if (dir_len == 0) {
    strcpy(path, name);
  } else {
    memcpy(path, dir, dir_len);
    path[dir_len] = '/';
    strcpy(path + dir_len + 1, name);
  }
  return path;
}

char* tags_find_file(const char* edited_path) {
  if (edited_path != NULL) {
    const char* slash = strrchr(edited_path, '/');
    /* A path with no directory part lives in the working directory, which
     * is checked last anyway. "/x" keeps its root as "" + "/tags" below. */
    size_t dir_len = slash ? (size_t)(slash - edited_path) : 0;
    while (slash != NULL) {
      char* candidate;
      if (dir_len == 0) {
        candidate = strdup("/tags");
      } else {
        candidate = tags_path_in(edited_path, dir_len);
      }
      if (candidate == NULL) {
        return NULL;
      }
      if (tags_file_readable(candidate)) {
        return candidate;
      }
      free(candidate);
      if (dir_len == 0) {
        break;  // just tried /tags
      }
      /* Step to the parent; a relative path stops at its first component. */
      const char* prev = NULL;
      for (const char* p = edited_path; p < edited_path + dir_len; ++p) {
        if (*p == '/') {
          prev = p;
        }
      }
      slash = prev;
      dir_len = prev ? (size_t)(prev - edited_path) : 0;
    }
  }
  if (tags_file_readable("tags")) {
    return strdup("tags");
  }
  return NULL;
}

/* Joins the tags file's directory and a filename from inside it. Absolute
 * names stay as they are, and "./" is dropped so a file reached through the
 * tags file matches the name it was opened under. */
static char* tags_resolve_filename(const char* tags_dir, const char* file, size_t len) {
  while (len >= 2 && file[0] == '.' && file[1] == '/') {
    file += 2;
    len -= 2;
  }
  if (file[0] == '/' || tags_dir == NULL || tags_dir[0] == '\0' ||
      strcmp(tags_dir, ".") == 0) {
    return tags_strndup(file, len);
  }
  size_t dir_len = strlen(tags_dir);
  const bool add_separator = tags_dir[dir_len - 1] != '/';  // "/" is already one
  char* path = (char*)malloc(dir_len + 1 + len + 1);
  if (path == NULL) {
    return NULL;
  }
  memcpy(path, tags_dir, dir_len);
  if (add_separator) {
    path[dir_len++] = '/';
  }
  memcpy(path + dir_len, file, len);
  path[dir_len + len] = '\0';
  return path;
}

/* Reads a /pattern/ or ?pattern? address starting at its delimiter. Only
 * the delimiter and backslash are escaped by ctags; any other backslash is
 * part of the text. */
static bool tags_parse_pattern(const char* p, TagEntry* out) {
  const char delimiter = *p++;
  size_t capacity = strlen(p) + 1;
  char* text = (char*)malloc(capacity);
  size_t n = 0;
  if (text == NULL) {
    return false;
  }
  if (*p == '^') {
    out->anchor_start = true;
    ++p;
  }
  for (;;) {
    if (*p == '\0') {
      free(text);
      return false;  // unterminated
    }
    if (*p == delimiter) {
      break;
    }
    if (*p == '\\' && (p[1] == delimiter || p[1] == '\\')) {
      text[n++] = p[1];
      p += 2;
      continue;
    }
    text[n++] = *p++;
  }
  /* An escape only ever yields the delimiter or a backslash, so a final `$`
   * is always the anchor. */
  if (n > 0 && text[n - 1] == '$') {
    out->anchor_end = true;
    --n;
  }
  text[n] = '\0';
  out->pattern = text;
  return true;
}

bool tags_parse_line(const char* line, const char* tags_dir, TagEntry* out) {
  memset(out, 0, sizeof(*out));
  if (line == NULL || line[0] == '\0' || line[0] == '!') {
    return false;  // empty, or a !_TAG_ pseudo-tag
  }
  const char* name_end = strchr(line, '\t');
  if (name_end == NULL || name_end == line) {
    return false;
  }
  const char* file = name_end + 1;
  const char* file_end = strchr(file, '\t');
  if (file_end == NULL || file_end == file) {
    return false;
  }
  const char* address = file_end + 1;

  if (*address == '/' || *address == '?') {
    if (!tags_parse_pattern(address, out)) {
      return false;
    }
  } else if (*address >= '1' && *address <= '9') {
    out->line = atoi(address);
  } else {
    return false;
  }

  out->name = tags_strndup(line, (size_t)(name_end - line));
  out->filename = tags_resolve_filename(tags_dir, file, (size_t)(file_end - file));
  if (out->name == NULL || out->filename == NULL) {
    tags_clear_entry(out);
    return false;
  }
  return true;
}

int tags_lookup(const char* tags_path, const char* name, TagEntry** out) {
  *out = NULL;
  if (tags_path == NULL || name == NULL || name[0] == '\0') {
    return 0;
  }
  FILE* file = fopen(tags_path, "r");
  if (file == NULL) {
    return 0;
  }

  const char* slash = strrchr(tags_path, '/');
  char* tags_dir;
  if (slash == NULL) {
    tags_dir = strdup("");
  } else if (slash == tags_path) {
    tags_dir = strdup("/");  // /tags at the root
  } else {
    tags_dir = tags_strndup(tags_path, (size_t)(slash - tags_path));
  }

  const size_t name_len = strlen(name);
  TagEntry* entries = NULL;
  int count = 0;
  char* line = NULL;
  size_t capacity = 0;
  ssize_t length;
  while ((length = getline(&line, &capacity, file)) != -1) {
    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
      line[--length] = '\0';
    }
    if (strncmp(line, name, name_len) != 0 || line[name_len] != '\t') {
      continue;
    }
    TagEntry entry;
    if (!tags_parse_line(line, tags_dir, &entry)) {
      continue;
    }
    TagEntry* grown = (TagEntry*)realloc(entries, sizeof(TagEntry) * (count + 1));
    if (grown == NULL) {
      tags_clear_entry(&entry);
      break;
    }
    entries = grown;
    entries[count++] = entry;
  }
  free(line);
  free(tags_dir);
  fclose(file);
  *out = entries;
  return count;
}

int tags_for_each_name(const char* tags_path,
                       const char* prefix,
                       bool (*visit)(void* context, const char* name, size_t len),
                       void* context) {
  if (tags_path == NULL || prefix == NULL) {
    return 0;
  }
  FILE* file = fopen(tags_path, "r");
  if (file == NULL) {
    return 0;
  }
  const size_t prefix_len = strlen(prefix);
  int count = 0;
  char* line = NULL;
  size_t capacity = 0;
  while (getline(&line, &capacity, file) != -1) {
    if (strncmp(line, "!_TAG_", 6) == 0 || strncmp(line, prefix, prefix_len) != 0) {
      continue;
    }
    const char* tab = strchr(line, '\t');
    if (tab == NULL || tab == line) {
      continue;  // not a tag line
    }
    ++count;
    if (!visit(context, line, (size_t)(tab - line))) {
      break;
    }
  }
  free(line);
  fclose(file);
  return count;
}

void tags_free_entries(TagEntry* entries, int count) {
  if (entries == NULL) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    tags_clear_entry(&entries[i]);
  }
  free(entries);
}

static bool tags_row_matches(const TagEntry* entry, const char* text, size_t len) {
  const size_t plen = strlen(entry->pattern);
  if (entry->anchor_start && entry->anchor_end) {
    return len == plen && memcmp(text, entry->pattern, plen) == 0;
  }
  if (plen > len) {
    return false;
  }
  if (entry->anchor_start) {
    return memcmp(text, entry->pattern, plen) == 0;
  }
  if (entry->anchor_end) {
    return memcmp(text + len - plen, entry->pattern, plen) == 0;
  }
  for (size_t i = 0; i + plen <= len; ++i) {
    if (memcmp(text + i, entry->pattern, plen) == 0) {
      return true;
    }
  }
  return false;
}

int tags_find_line(const TagEntry* entry, const Buffer* buffer) {
  if (entry == NULL || buffer == NULL) {
    return -1;
  }
  const int number_of_lines = buffer_get_number_of_lines(buffer);
  if (entry->pattern == NULL) {
    if (entry->line <= 0 || number_of_lines <= 0) {
      return -1;
    }
    return (entry->line > number_of_lines ? number_of_lines : entry->line) - 1;
  }
  int index = 0;
  for (const BufferRow* row = buffer_get_first_row(buffer); row != NULL;
       row = buffer_row_get_next(row), ++index) {
    size_t len = row->len > 0 ? (size_t)row->len : 0;
    while (len > 0 && (row->data[len - 1] == '\n' || row->data[len - 1] == '\r')) {
      --len;
    }
    if (tags_row_matches(entry, row->data, len)) {
      return index;
    }
  }
  return -1;
}
