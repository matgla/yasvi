/*
 Copyright (c) 2025 Mateusz Stadnik

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

#include "filetype.h"

#include <string.h>

#include "../buffer_row.h"

// Include built-in filetype definitions
#include "c.h"
#include "shell.h"

// Registry of all built-in filetypes
const Filetype* g_filetypes[] = {
  &g_filetype_c,
  &g_filetype_shell,
};

const size_t g_num_filetypes = sizeof(g_filetypes) / sizeof(g_filetypes[0]);

const Filetype* filetype_by_extension(const char* filename) {
  if (!filename) {
    return NULL;
  }

  // Find the last dot in filename
  const char* ext = strrchr(filename, '.');
  if (!ext) {
    return NULL;
  }

  // Search through all filetypes
  for (size_t i = 0; i < g_num_filetypes; ++i) {
    const Filetype* ft = g_filetypes[i];
    if (!ft->extensions) {
      continue;
    }

    for (const char** e = ft->extensions; *e != NULL; ++e) {
      if (strcmp(ext, *e) == 0) {
        return ft;
      }
    }
  }

  return NULL;
}

const Filetype* filetype_by_shebang(const char* first_line) {
  if (!first_line || strncmp(first_line, "#!", 2) != 0) {
    return NULL;
  }

  // Skip "#!" and whitespace
  const char* cmd = first_line + 2;
  while (*cmd == ' ' || *cmd == '\t') {
    ++cmd;
  }

  // Search through all filetypes
  for (size_t i = 0; i < g_num_filetypes; ++i) {
    const Filetype* ft = g_filetypes[i];
    if (!ft->shebangs) {
      continue;
    }

    for (const char** s = ft->shebangs; *s != NULL; ++s) {
      // Check if shebang matches (prefix match for things like "/bin/bash" vs "/usr/bin/env bash")
      if (strstr(cmd, *s) != NULL) {
        return ft;
      }
    }
  }

  return NULL;
}

const Filetype* filetype_detect(const char* filename, const char* first_line) {
  // 1. Try extension match first
  const Filetype* ft = filetype_by_extension(filename);
  if (ft) {
    return ft;
  }

  // 2. Try shebang match
  if (first_line) {
    ft = filetype_by_shebang(first_line);
    if (ft) {
      return ft;
    }
  }

  // 3. No match found - plain text
  return NULL;
}

void buffer_row_highlight_with_filetype(BufferRow* row, const Filetype* filetype) {
  if (!row) {
    return;
  }

  // Use C highlighter as default if no filetype is set
  const Filetype* ft = filetype ? filetype : &g_filetype_c;

  if (!ft->highlighter) {
    // No highlighter - set all to Normal
    for (int i = 0; i < row->len; ++i) {
      row->highlight_data[i] = (char)EHighlightToken_Normal;
    }
    return;
  }

  // Prepare context
  HighlightContext ctx = {
    .line = row->data,
    .len = row->len,
    .tokens = row->highlight_data,
    .in_multiline_comment = row->highlight_comment_open,
    .in_string = row->highlight_string_open != 0,
    .string_char = row->highlight_string_open,
  };

  // Call the highlighter
  ft->highlighter->highlight_line(&ctx);

  // Update multiline state for next line
  row->highlight_comment_open = ctx.in_multiline_comment;
  row->highlight_string_open = ctx.in_string ? ctx.string_char : 0;
}
