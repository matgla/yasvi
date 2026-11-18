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

// The editor's own settings (everything but :make's, which make.h keeps).
// They come from the user's config file, CONFIG_FILE_NAME in
// $XDG_CONFIG_HOME, else in $HOME/.config (/root/.config when HOME is
// unset): lines "key=value" or "set key=value", # comments. :set changes
// them for this session.
//
// Settings:
//   record / norecord / record=on|off  keep every key typed in a file, with
//                                      its timing, in FILE.rec (record.h)
//   autoindent / noautoindent (ai)     indent new lines and closing brackets
//                                      (indent.h); on unless turned off, off
//                                      for pasting into the terminal

#define CONFIG_FILE_NAME "vi.conf"

typedef struct {
  bool record;
  bool autoindent;
} EditorConfig;

typedef enum {
  ConfigSet_Ok,
  ConfigSet_Ignored,     // blank or a # comment
  ConfigSet_Query,       // "record?": show the value
  ConfigSet_UnknownKey,  // not one of ours (make_config_set may know it)
  ConfigSet_BadValue,    // record=maybe
} ConfigSetResult;

void config_init(EditorConfig* config);

// One setting, an rc line or :set's argument: "key=value", "set key=value",
// and for a boolean also "key" (on), "nokey" (off) and "key?" (query).
ConfigSetResult config_set(EditorConfig* config, const char* line);

// Every setting, as "norecord autoindent", into `out`.
void config_describe(const EditorConfig* config, char* out, size_t size);

// The setting a config_set `line` names, as "noautoindent", into `out`
// (every setting when it names none).
void config_describe_setting(const EditorConfig* config,
                             const char* line,
                             char* out,
                             size_t size);

// The user's config file: `xdg_config_home`/vi.conf, else
// `home`/.config/vi.conf; either may be NULL or empty (then /root is the
// home). malloc'd; NULL only when out of memory.
char* config_user_path(const char* xdg_config_home, const char* home);

// Calls `apply` for every line of the file at `path`; `apply` returns false
// for a line it did not understand, which *bad_lines (may be NULL) counts.
// False if the file cannot be read.
bool config_read_lines(const char* path,
                       bool (*apply)(void* context, const char* line),
                       void* context,
                       int* bad_lines);
