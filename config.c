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
  bool inverted;           // the field holds the opposite (noexpandtab)
} ConfigBool;

static const ConfigBool g_config_bools[] = {
  {"record", NULL, offsetof(EditorConfig, record), false},
  {"autoindent", "ai", offsetof(EditorConfig, autoindent), false},
  {"expandtab", "et", offsetof(EditorConfig, noexpandtab), true},
};

#define CONFIG_BOOL_COUNT (sizeof(g_config_bools) / sizeof(g_config_bools[0]))

typedef struct {
  const char* name;
  const char* short_name;
  size_t offset;  // of the unsigned char in EditorConfig
  int min;
  int max;
} ConfigInt;

static const ConfigInt g_config_ints[] = {
  {"tabstop", "ts", offsetof(EditorConfig, tabstop), 1, CONFIG_MAX_WIDTH},
  {"shiftwidth", "sw", offsetof(EditorConfig, shiftwidth), 0, CONFIG_MAX_WIDTH},
};

#define CONFIG_INT_COUNT (sizeof(g_config_ints) / sizeof(g_config_ints[0]))

static bool config_bool_get(const EditorConfig* config, const ConfigBool* option) {
  const bool value = *(const bool*)((const char*)config + option->offset);
  return option->inverted ? !value : value;
}

static void config_bool_put(EditorConfig* config, const ConfigBool* option, bool value) {
  *(bool*)((char*)config + option->offset) = option->inverted ? !value : value;
}

static unsigned char* config_int_at(EditorConfig* config, const ConfigInt* option) {
  return (unsigned char*)config + option->offset;
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

static bool config_names(const char* key,
                         size_t length,
                         const char* name,
                         const char* short_name) {
  return config_word_is(key, length, name) ||
         (short_name != NULL && config_word_is(key, length, short_name));
}

static const ConfigBool* config_find_bool(const char* key, size_t length) {
  for (size_t i = 0; i < CONFIG_BOOL_COUNT; ++i) {
    if (config_names(key, length, g_config_bools[i].name, g_config_bools[i].short_name)) {
      return &g_config_bools[i];
    }
  }
  return NULL;
}

static const ConfigInt* config_find_int(const char* key, size_t length) {
  for (size_t i = 0; i < CONFIG_INT_COUNT; ++i) {
    if (config_names(key, length, g_config_ints[i].name, g_config_ints[i].short_name)) {
      return &g_config_ints[i];
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
  config->noexpandtab = false;
  config->tabstop = CONFIG_DEFAULT_TABSTOP;
  config->shiftwidth = 0;
}

// The setting `line` names, past an optional "set": its key and where the
// text after the key starts.
typedef struct {
  const char* key;
  size_t length;
  const char* rest;
  const ConfigBool* boolean;  // one of these two, or neither
  const ConfigInt* integer;
  bool negated;  // "nokey"
} ConfigKey;

static ConfigKey config_parse_key(const char* line) {
  ConfigKey parsed = {0};
  const char* p = config_skip_spaces(line);
  if (strncmp(p, "set", 3) == 0 && (p[3] == ' ' || p[3] == '\t')) {
    p = config_skip_spaces(p + 3);
  }
  parsed.key = p;
  while (*p != '\0' && *p != '=' && *p != '?' && *p != '!' && !isspace((unsigned char)*p)) {
    ++p;
  }
  parsed.length = (size_t)(p - parsed.key);
  parsed.rest = p;
  parsed.boolean = config_find_bool(parsed.key, parsed.length);
  parsed.integer = parsed.boolean == NULL ? config_find_int(parsed.key, parsed.length) : NULL;
  if (parsed.boolean == NULL && parsed.integer == NULL && parsed.length > 2 &&
      strncmp(parsed.key, "no", 2) == 0) {
    parsed.boolean = config_find_bool(parsed.key + 2, parsed.length - 2);
    parsed.negated = parsed.boolean != NULL;
  }
  return parsed;
}

static ConfigSetResult config_set_int(EditorConfig* config, const ConfigKey* parsed) {
  const char* p = config_skip_spaces(parsed->rest);
  if (*p == '?' || config_trimmed_length(p) == 0) {
    // "tabstop?" and "tabstop" show the value
    return config_trimmed_length(*p == '?' ? p + 1 : p) == 0 ? ConfigSet_Query
                                                            : ConfigSet_UnknownKey;
  }
  if (*p != '=') {
    return ConfigSet_UnknownKey;
  }
  const char* value = config_skip_spaces(p + 1);
  const size_t length = config_trimmed_length(value);
  int number = 0;
  if (length == 0 || length > 3) {
    return ConfigSet_BadValue;
  }
  for (size_t i = 0; i < length; ++i) {
    if (!isdigit((unsigned char)value[i])) {
      return ConfigSet_BadValue;
    }
    number = number * 10 + (value[i] - '0');
  }
  if (number < parsed->integer->min || number > parsed->integer->max) {
    return ConfigSet_BadValue;
  }
  *config_int_at(config, parsed->integer) = (unsigned char)number;
  return ConfigSet_Ok;
}

ConfigSetResult config_set(EditorConfig* config, const char* line) {
  const char* p = config_skip_spaces(line);
  if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#') {
    return ConfigSet_Ignored;
  }
  const ConfigKey parsed = config_parse_key(line);
  if (parsed.integer != NULL) {
    return config_set_int(config, &parsed);
  }
  const ConfigBool* option = parsed.boolean;
  if (option == NULL) {
    return ConfigSet_UnknownKey;
  }
  p = config_skip_spaces(parsed.rest);
  if ((*p == '?' || *p == '!') && !parsed.negated) {
    if (config_trimmed_length(p + 1) != 0) {
      return ConfigSet_UnknownKey;
    }
    if (*p == '?') {
      return ConfigSet_Query;
    }
    config_bool_put(config, option, !config_bool_get(config, option));  // "et!"
    return ConfigSet_Ok;
  }
  if (*p != '=') {
    if (config_trimmed_length(p) != 0) {
      return ConfigSet_UnknownKey;
    }
    config_bool_put(config, option, !parsed.negated);  // "record", "norecord"
    return ConfigSet_Ok;
  }
  if (parsed.negated) {
    return ConfigSet_BadValue;  // "norecord=on"
  }
  const char* value = config_skip_spaces(p + 1);
  bool enabled;
  if (!config_parse_bool(value, config_trimmed_length(value), &enabled)) {
    return ConfigSet_BadValue;
  }
  config_bool_put(config, option, enabled);
  return ConfigSet_Ok;
}

ConfigSetting config_setting_of(const char* line) {
  const ConfigKey parsed = config_parse_key(line);
  if (parsed.boolean != NULL && parsed.boolean->offset == offsetof(EditorConfig, noexpandtab)) {
    return ConfigSetting_Expandtab;
  }
  if (parsed.integer != NULL) {
    return parsed.integer->offset == offsetof(EditorConfig, shiftwidth)
             ? ConfigSetting_Shiftwidth
             : ConfigSetting_Tabstop;
  }
  return parsed.boolean != NULL ? ConfigSetting_Other : ConfigSetting_Unknown;
}

static int config_describe_bool(const EditorConfig* config,
                                const ConfigBool* option,
                                char* out,
                                size_t size) {
  return snprintf(out, size, "%s%s", config_bool_get(config, option) ? "" : "no",
                  option->name);
}

static int config_describe_int(const EditorConfig* config,
                               const ConfigInt* option,
                               char* out,
                               size_t size) {
  int value = *config_int_at((EditorConfig*)config, option);
  if (value == 0 && option->min > 0) {
    value = CONFIG_DEFAULT_TABSTOP;  // a zeroed config: the default
  }
  return snprintf(out, size, "%s=%d", option->name, value);
}

void config_describe(const EditorConfig* config, char* out, size_t size) {
  size_t used = 0;
  if (size == 0) {
    return;
  }
  out[0] = '\0';
  for (size_t i = 0; i < CONFIG_BOOL_COUNT + CONFIG_INT_COUNT && used + 1 < size; ++i) {
    if (used > 0) {
      out[used++] = ' ';
      out[used] = '\0';
    }
    const int written =
      i < CONFIG_BOOL_COUNT
        ? config_describe_bool(config, &g_config_bools[i], out + used, size - used)
        : config_describe_int(config, &g_config_ints[i - CONFIG_BOOL_COUNT], out + used,
                              size - used);
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
  const ConfigKey parsed = config_parse_key(line);
  if (parsed.boolean != NULL) {
    config_describe_bool(config, parsed.boolean, out, size);
  } else if (parsed.integer != NULL) {
    config_describe_int(config, parsed.integer, out, size);
  } else {
    config_describe(config, out, size);
  }
}

void config_expected_value(const char* line, char* out, size_t size) {
  const ConfigKey parsed = config_parse_key(line);
  if (parsed.integer != NULL) {
    snprintf(out, size, "%s is a number from %d to %d", parsed.integer->name,
             parsed.integer->min, parsed.integer->max);
  } else if (parsed.boolean != NULL) {
    snprintf(out, size, "%s is on or off", parsed.boolean->name);
  } else {
    snprintf(out, size, "unknown setting");
  }
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
