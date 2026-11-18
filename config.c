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

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char* name;
  const char* short_name;  // vim's abbreviation, or NULL
  size_t offset;           // of the bool in EditorConfig
} ConfigBool;

static const ConfigBool g_config_bools[] = {
  {"record", NULL, offsetof(EditorConfig, record)},
  {"autoindent", "ai", offsetof(EditorConfig, autoindent)},
};

#define CONFIG_BOOL_COUNT (sizeof(g_config_bools) / sizeof(g_config_bools[0]))

static bool* config_bool_at(EditorConfig* config, const ConfigBool* option) {
  return (bool*)((char*)config + option->offset);
}

static const char* config_skip_spaces(const char* p) {
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  return p;
}

// Length of `text` without trailing whitespace and line ends.
static size_t config_trimmed_length(const char* text) {
  size_t length = strlen(text);
  while (length > 0 && isspace((unsigned char)text[length - 1])) {
    --length;
  }
  return length;
}

static bool config_word_is(const char* word, size_t length, const char* name) {
  return strlen(name) == length && strncmp(word, name, length) == 0;
}

static const ConfigBool* config_find_bool(const char* key, size_t length) {
  for (size_t i = 0; i < CONFIG_BOOL_COUNT; ++i) {
    if (config_word_is(key, length, g_config_bools[i].name) ||
        (g_config_bools[i].short_name != NULL &&
         config_word_is(key, length, g_config_bools[i].short_name))) {
      return &g_config_bools[i];
    }
  }
  return NULL;
}

// on/off and the usual spellings of them. False when `value` is neither.
static bool config_parse_bool(const char* value, size_t length, bool* out) {
  static const char* const on[] = {"on", "yes", "true", "1"};
  static const char* const off[] = {"off", "no", "false", "0"};
  for (size_t i = 0; i < sizeof(on) / sizeof(on[0]); ++i) {
    if (config_word_is(value, length, on[i])) {
      *out = true;
      return true;
    }
    if (config_word_is(value, length, off[i])) {
      *out = false;
      return true;
    }
  }
  return false;
}

void config_init(EditorConfig* config) {
  config->record = false;
  config->autoindent = true;
}

ConfigSetResult config_set(EditorConfig* config, const char* line) {
  const char* p = config_skip_spaces(line);
  if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#') {
    return ConfigSet_Ignored;
  }
  if (strncmp(p, "set", 3) == 0 && (p[3] == ' ' || p[3] == '\t')) {
    p = config_skip_spaces(p + 3);
  }
  const char* key = p;
  while (*p != '\0' && *p != '=' && *p != '?' && !isspace((unsigned char)*p)) {
    ++p;
  }
  size_t key_length = (size_t)(p - key);
  const ConfigBool* option = config_find_bool(key, key_length);
  bool negated = false;
  if (option == NULL && key_length > 2 && strncmp(key, "no", 2) == 0) {
    option = config_find_bool(key + 2, key_length - 2);
    negated = true;
  }
  if (option == NULL) {
    return ConfigSet_UnknownKey;
  }
  p = config_skip_spaces(p);
  if (*p == '?' && !negated) {
    return config_trimmed_length(p + 1) == 0 ? ConfigSet_Query
                                             : ConfigSet_UnknownKey;
  }
  if (*p != '=') {
    if (config_trimmed_length(p) != 0) {
      return ConfigSet_UnknownKey;
    }
    *config_bool_at(config, option) = !negated;  // "record", "norecord"
    return ConfigSet_Ok;
  }
  if (negated) {
    return ConfigSet_BadValue;  // "norecord=on"
  }
  const char* value = config_skip_spaces(p + 1);
  bool enabled;
  if (!config_parse_bool(value, config_trimmed_length(value), &enabled)) {
    return ConfigSet_BadValue;
  }
  *config_bool_at(config, option) = enabled;
  return ConfigSet_Ok;
}

void config_describe(const EditorConfig* config, char* out, size_t size) {
  size_t used = 0;
  if (size == 0) {
    return;
  }
  out[0] = '\0';
  for (size_t i = 0; i < CONFIG_BOOL_COUNT && used < size; ++i) {
    const ConfigBool* option = &g_config_bools[i];
    const bool enabled = *config_bool_at((EditorConfig*)config, option);
    const int written = snprintf(out + used, size - used, "%s%s%s",
                                 used > 0 ? " " : "", enabled ? "" : "no",
                                 option->name);
    if (written < 0) {
      break;
    }
    used += (size_t)written;
  }
}

void config_describe_setting(const EditorConfig* config,
                             const char* line,
                             char* out,
                             size_t size) {
  if (size == 0) {
    return;
  }
  const char* p = config_skip_spaces(line);
  if (strncmp(p, "set", 3) == 0 && (p[3] == ' ' || p[3] == '\t')) {
    p = config_skip_spaces(p + 3);
  }
  const char* key = p;
  while (*p != '\0' && *p != '=' && *p != '?' && !isspace((unsigned char)*p)) {
    ++p;
  }
  const size_t key_length = (size_t)(p - key);
  const ConfigBool* option = config_find_bool(key, key_length);
  if (option == NULL && key_length > 2 && strncmp(key, "no", 2) == 0) {
    option = config_find_bool(key + 2, key_length - 2);
  }
  if (option == NULL) {
    config_describe(config, out, size);
    return;
  }
  const bool enabled = *config_bool_at((EditorConfig*)config, option);
  snprintf(out, size, "%s%s", enabled ? "" : "no", option->name);
}

char* config_user_path(const char* xdg_config_home, const char* home) {
  const char* base = xdg_config_home;
  const char* middle = "/";
  if (base == NULL || base[0] == '\0') {
    base = home != NULL && home[0] != '\0' ? home : "/root";  // as autosave.c
    middle = "/.config/";
  }
  const size_t length = strlen(base) + strlen(middle) + strlen(CONFIG_FILE_NAME);
  char* path = malloc(length + 1);
  if (path != NULL) {
    snprintf(path, length + 1, "%s%s%s", base, middle, CONFIG_FILE_NAME);
  }
  return path;
}

bool config_read_lines(const char* path,
                       bool (*apply)(void* context, const char* line),
                       void* context,
                       int* bad_lines) {
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  int bad = 0;
  char* line = NULL;
  size_t capacity = 0;
  while (getline(&line, &capacity, file) != -1) {
    if (!apply(context, line)) {
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
