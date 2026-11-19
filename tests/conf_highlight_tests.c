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

// The conf filetype (filetypes/conf.c): vi.conf, .yasvirc and other
// configuration files.

#include "acutest.h"

#include "buffer.h"
#include "filetypes/conf.h"

#include <string.h>

static Buffer* conf_buffer(const char* line) {
  Buffer* buffer = buffer_alloc();
  buffer->filetype = &g_filetype_conf;
  buffer_append_line(buffer, line);
  return buffer;
}

// Columns [start, end) all carry `token`.
static void check_range(const BufferRow* row, int start, int end, EHighlightToken token) {
  for (int i = start; i < end; ++i) {
    TEST_CHECK_(row->highlight_data[i] == (char)token, "column %d of '%s' is %d, expected %d",
                i, row->data, row->highlight_data[i], token);
  }
}

void test_conf_detected(void) {
  TEST_CHECK(filetype_by_extension("/root/.config/vi.conf") == &g_filetype_conf);
  TEST_CHECK(filetype_by_extension(".yasvirc") == &g_filetype_conf);
  TEST_CHECK(filetype_by_extension("/home/me/.yasvirc") == &g_filetype_conf);
  TEST_CHECK(filetype_by_extension("setup.cfg") == &g_filetype_conf);
  TEST_CHECK(filetype_by_extension("/etc/fstab") == &g_filetype_conf);
  TEST_CHECK(filetype_detect("etc/inittab", NULL) == &g_filetype_conf);
  TEST_CHECK(filetype_by_extension("fstab.c") == &g_filetype_c);
  TEST_CHECK(filetype_by_extension("my.fstab") == NULL);
  // the extension is taken from the name, not from a directory
  TEST_CHECK(filetype_by_extension("build.d/README") == NULL);
}

void test_conf_comment_and_section(void) {
  Buffer* buffer = conf_buffer("  # set record");
  check_range(buffer->current_row, 0, 2, EHighlightToken_Normal);
  check_range(buffer->current_row, 2, 14, EHighlightToken_Comment);
  buffer_free(buffer);

  buffer = conf_buffer("; ini");
  check_range(buffer->current_row, 0, 5, EHighlightToken_Comment);
  buffer_free(buffer);

  buffer = conf_buffer("[core]");
  check_range(buffer->current_row, 0, 1, EHighlightToken_Symbol);
  check_range(buffer->current_row, 1, 5, EHighlightToken_Type);
  check_range(buffer->current_row, 5, 6, EHighlightToken_Symbol);
  buffer_free(buffer);
}

void test_conf_vi_settings(void) {
  // set record
  Buffer* buffer = conf_buffer("set record");
  check_range(buffer->current_row, 0, 3, EHighlightToken_Preprocessor);
  check_range(buffer->current_row, 4, 10, EHighlightToken_Keyword);
  buffer_free(buffer);

  // noautoindent: "no" and the setting
  buffer = conf_buffer("noautoindent");
  check_range(buffer->current_row, 0, 2, EHighlightToken_Digit);
  check_range(buffer->current_row, 2, 12, EHighlightToken_Keyword);
  buffer_free(buffer);

  // record=on
  buffer = conf_buffer("record=on");
  check_range(buffer->current_row, 0, 6, EHighlightToken_Keyword);
  check_range(buffer->current_row, 6, 7, EHighlightToken_Symbol);
  check_range(buffer->current_row, 7, 9, EHighlightToken_Digit);
  buffer_free(buffer);

  // set tabstop=4, noet
  buffer = conf_buffer("set tabstop=4");
  check_range(buffer->current_row, 4, 11, EHighlightToken_Keyword);
  check_range(buffer->current_row, 12, 13, EHighlightToken_Digit);
  buffer_free(buffer);
  buffer = conf_buffer("noet");
  check_range(buffer->current_row, 0, 2, EHighlightToken_Digit);
  check_range(buffer->current_row, 2, 4, EHighlightToken_Keyword);
  buffer_free(buffer);

  // a misspelt setting is not a keyword
  buffer = conf_buffer("set recrod");
  check_range(buffer->current_row, 4, 10, EHighlightToken_Type);
  buffer_free(buffer);

  // makeprg=tcc -o x % ; makemode=exec
  buffer = conf_buffer("makeprg=tcc -o x %");
  check_range(buffer->current_row, 0, 7, EHighlightToken_Keyword);
  check_range(buffer->current_row, 8, 17, EHighlightToken_Normal);
  check_range(buffer->current_row, 17, 18, EHighlightToken_Symbol2);
  buffer_free(buffer);
}

void test_conf_values(void) {
  Buffer* buffer = conf_buffer("name = \"a b\" $HOME 42 true");
  BufferRow* row = buffer->current_row;
  check_range(row, 0, 4, EHighlightToken_Type);
  check_range(row, 5, 6, EHighlightToken_Symbol);
  check_range(row, 7, 12, EHighlightToken_String);
  check_range(row, 13, 18, EHighlightToken_Preprocessor);
  check_range(row, 19, 21, EHighlightToken_Digit);
  check_range(row, 22, 26, EHighlightToken_Digit);
  buffer_free(buffer);

  // fstab: LABEL=boot /boot vfat defaults 0 2
  buffer = conf_buffer("LABEL=boot /boot vfat defaults 0 2");
  row = buffer->current_row;
  check_range(row, 0, 5, EHighlightToken_Type);
  check_range(row, 5, 6, EHighlightToken_Symbol);
  check_range(row, 6, 31, EHighlightToken_Normal);
  check_range(row, 31, 32, EHighlightToken_Digit);
  check_range(row, 33, 34, EHighlightToken_Digit);
  buffer_free(buffer);
}

TEST_LIST = {
  {"conf_detected", test_conf_detected},
  {"conf_comment_and_section", test_conf_comment_and_section},
  {"conf_vi_settings", test_conf_vi_settings},
  {"conf_values", test_conf_values},
  {NULL, NULL},
};
