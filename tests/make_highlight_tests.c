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

// The make filetype (filetypes/makefile.c): detection, highlighting, the
// state carried across lines, and the rule lines a recipe follows.

#include "acutest.h"

#include "buffer.h"
#include "filetypes/makefile.h"

#include <string.h>

// A buffer of the given lines (NULL-terminated), highlighted top-down.
static Buffer* make_buffer(const char* const* lines) {
  Buffer* buffer = buffer_alloc();
  buffer->filetype = &g_filetype_make;
  for (const char* const* line = lines; *line != NULL; ++line) {
    buffer_append_line(buffer, *line);
  }
  return buffer;
}

static Buffer* make_line(const char* line) {
  const char* lines[] = {line, NULL};
  return make_buffer(lines);
}

static const BufferRow* row_at(const Buffer* buffer, int index) {
  const BufferRow* row = buffer_get_first_row(buffer);
  while (row != NULL && index-- > 0) {
    row = row->next;
  }
  TEST_ASSERT(row != NULL);
  return row;
}

// Columns [start, end) all carry `token`.
static void check_range(const BufferRow* row, int start, int end, EHighlightToken token) {
  for (int i = start; i < end; ++i) {
    TEST_CHECK_(row->highlight_data[i] == (char)token, "column %d of '%s' is %d, expected %d",
                i, row->data, row->highlight_data[i], token);
  }
}

void test_make_detected(void) {
  TEST_CHECK(filetype_by_extension("Makefile") == &g_filetype_make);
  TEST_CHECK(filetype_by_extension("apps/ccat/Makefile") == &g_filetype_make);
  TEST_CHECK(filetype_by_extension("/src/makefile") == &g_filetype_make);
  TEST_CHECK(filetype_by_extension("GNUmakefile") == &g_filetype_make);
  TEST_CHECK(filetype_by_extension("Makefile.in") == &g_filetype_make);
  TEST_CHECK(filetype_by_extension("rules.mk") == &g_filetype_make);
  TEST_CHECK(filetype_by_extension("config.mak") == &g_filetype_make);
  TEST_CHECK(filetype_detect("Makefile", "all:") == &g_filetype_make);
  TEST_CHECK(filetype_by_extension("Makefile.c") == &g_filetype_c);
  TEST_CHECK(filetype_by_extension("NotAMakefile") == NULL);
  TEST_CHECK(g_filetype_make.hard_tabs);
  TEST_CHECK(!g_filetype_c.hard_tabs);
}

void test_make_assignment(void) {
  // CC ?= armv8m-tcc
  Buffer* buffer = make_line("CC ?= armv8m-tcc");
  check_range(buffer->current_row, 0, 2, EHighlightToken_Type);
  check_range(buffer->current_row, 2, 3, EHighlightToken_Normal);
  check_range(buffer->current_row, 3, 5, EHighlightToken_Symbol);
  check_range(buffer->current_row, 5, 16, EHighlightToken_Normal);
  buffer_free(buffer);

  // OBJS := $(patsubst %.c,build/%.o,$(SRCS)) # objects
  buffer = make_line("OBJS := $(patsubst %.c,build/%.o,$(SRCS)) # objects");
  const BufferRow* row = buffer->current_row;
  check_range(row, 0, 4, EHighlightToken_Type);
  check_range(row, 5, 7, EHighlightToken_Symbol);
  check_range(row, 8, 10, EHighlightToken_Preprocessor);   // $(
  check_range(row, 10, 18, EHighlightToken_Keyword);       // patsubst
  check_range(row, 18, 22, EHighlightToken_Normal);        // " %.c"
  check_range(row, 22, 23, EHighlightToken_Symbol);        // ,
  check_range(row, 23, 32, EHighlightToken_Normal);        // build/%.o
  check_range(row, 32, 33, EHighlightToken_Symbol);        // ,
  check_range(row, 33, 40, EHighlightToken_Preprocessor);  // $(SRCS)
  check_range(row, 40, 41, EHighlightToken_Preprocessor);  // )
  check_range(row, 42, 51, EHighlightToken_Comment);
  buffer_free(buffer);

  // simple and append forms
  buffer = make_line("X::=1");
  check_range(buffer->current_row, 0, 1, EHighlightToken_Type);
  check_range(buffer->current_row, 1, 4, EHighlightToken_Symbol);
  buffer_free(buffer);
  buffer = make_line("LDFLAGS += -L$(DIR)");
  check_range(buffer->current_row, 0, 7, EHighlightToken_Type);
  check_range(buffer->current_row, 8, 10, EHighlightToken_Symbol);
  check_range(buffer->current_row, 13, 19, EHighlightToken_Preprocessor);
  buffer_free(buffer);
}

void test_make_rule_and_recipe(void) {
  const char* lines[] = {
    ".PHONY: all clean",
    "build/%.o: %.c | build",
    "\t@mkdir -p $(dir $@)",
    "\t$(CC) -c $< -o $@ # \"compile\"",
    NULL,
  };
  Buffer* buffer = make_buffer(lines);

  const BufferRow* row = row_at(buffer, 0);
  check_range(row, 0, 6, EHighlightToken_Keyword);  // .PHONY
  check_range(row, 6, 7, EHighlightToken_Symbol);
  check_range(row, 7, 17, EHighlightToken_Normal);

  row = row_at(buffer, 1);
  check_range(row, 0, 6, EHighlightToken_Keyword2);  // build/
  check_range(row, 6, 7, EHighlightToken_Symbol2);   // %
  check_range(row, 7, 9, EHighlightToken_Keyword2);  // .o
  check_range(row, 9, 10, EHighlightToken_Symbol);   // :
  check_range(row, 11, 12, EHighlightToken_Symbol2); // %
  check_range(row, 15, 16, EHighlightToken_Symbol);  // |

  row = row_at(buffer, 2);
  check_range(row, 1, 2, EHighlightToken_Symbol);          // @
  check_range(row, 11, 13, EHighlightToken_Preprocessor);  // $(
  check_range(row, 13, 16, EHighlightToken_Keyword);       // dir
  check_range(row, 17, 19, EHighlightToken_Preprocessor);  // $@
  check_range(row, 19, 20, EHighlightToken_Preprocessor);  // )

  row = row_at(buffer, 3);
  check_range(row, 1, 6, EHighlightToken_Preprocessor);   // $(CC)
  check_range(row, 10, 12, EHighlightToken_Preprocessor); // $<
  check_range(row, 16, 18, EHighlightToken_Preprocessor); // $@
  check_range(row, 19, 30, EHighlightToken_Comment);      // the shell's comment
  buffer_free(buffer);
}

void test_make_directives(void) {
  const char* lines[] = {
    "ifeq ($(CC), armv8m-tcc)",
    "-include $(OBJS:.o=.d)",
    "export PATH := /bin",
    "else ifneq \"a\" \"b\"",
    "endif",
    NULL,
  };
  Buffer* buffer = make_buffer(lines);
  const BufferRow* row = row_at(buffer, 0);
  check_range(row, 0, 4, EHighlightToken_Keyword);
  check_range(row, 6, 11, EHighlightToken_Preprocessor);

  row = row_at(buffer, 1);
  check_range(row, 0, 8, EHighlightToken_Keyword);
  check_range(row, 9, 22, EHighlightToken_Preprocessor);

  row = row_at(buffer, 2);
  check_range(row, 0, 6, EHighlightToken_Keyword);  // export
  check_range(row, 7, 11, EHighlightToken_Type);    // PATH
  check_range(row, 12, 14, EHighlightToken_Symbol);

  row = row_at(buffer, 3);
  check_range(row, 0, 4, EHighlightToken_Keyword);   // else
  check_range(row, 5, 10, EHighlightToken_Keyword);  // ifneq
  check_range(row, 11, 14, EHighlightToken_String);
  check_range(row, 15, 18, EHighlightToken_String);

  check_range(row_at(buffer, 4), 0, 5, EHighlightToken_Keyword);
  buffer_free(buffer);

  // "export = x" sets a variable called export
  buffer = make_line("export = x");
  check_range(buffer->current_row, 0, 6, EHighlightToken_Type);
  buffer_free(buffer);
}

void test_make_state_across_lines(void) {
  const char* lines[] = {
    "SRCS = a.c \\",
    "\t@b.c",  // a continued value, not a recipe
    "# a comment \\",
    "still the comment",
    "define RULE",
    "$(1): ; cc",
    "endef",
    "all:",
    "\techo 'a \\",
    "b' $(X)",  // the recipe and its quote go on
    "done: all",
    NULL,
  };
  Buffer* buffer = make_buffer(lines);
  check_range(row_at(buffer, 1), 0, 5, EHighlightToken_Normal);  // no @ symbol
  check_range(row_at(buffer, 3), 0, 17, EHighlightToken_Comment);

  const BufferRow* row = row_at(buffer, 4);
  check_range(row, 0, 6, EHighlightToken_Keyword);
  check_range(row, 7, 11, EHighlightToken_Type);
  row = row_at(buffer, 5);
  check_range(row, 0, 4, EHighlightToken_Preprocessor);  // $(1), no rule inside
  check_range(row, 4, 10, EHighlightToken_Normal);
  check_range(row_at(buffer, 6), 0, 5, EHighlightToken_Keyword);

  row = row_at(buffer, 9);
  check_range(row, 0, 2, EHighlightToken_String);
  check_range(row, 3, 7, EHighlightToken_Preprocessor);
  check_range(row_at(buffer, 10), 0, 4, EHighlightToken_Keyword2);  // back to rules
  buffer_free(buffer);
}

void test_make_is_rule(void) {
#define IS_RULE(s) makefile_is_rule(s, (int)strlen(s))
  TEST_CHECK(IS_RULE("all: build/vi"));
  TEST_CHECK(IS_RULE("build/%.o: %.c"));
  TEST_CHECK(IS_RULE("clean:"));
  TEST_CHECK(IS_RULE("a b:: c"));
  TEST_CHECK(IS_RULE("$(TARGET): $(OBJS)"));
  TEST_CHECK(!IS_RULE("CC := gcc"));
  TEST_CHECK(!IS_RULE("X ::= 1"));
  TEST_CHECK(!IS_RULE("prog: CFLAGS += -g"));
  TEST_CHECK(!IS_RULE("\t$(CC) -o $@ a: b"));
  TEST_CHECK(!IS_RULE("# all: x"));
  TEST_CHECK(!IS_RULE("ifeq ($(A),b:c)"));
  TEST_CHECK(!IS_RULE("include x.mk"));
  TEST_CHECK(!IS_RULE(""));
#undef IS_RULE
}

TEST_LIST = {
  {"make_detected", test_make_detected},
  {"make_assignment", test_make_assignment},
  {"make_rule_and_recipe", test_make_rule_and_recipe},
  {"make_directives", test_make_directives},
  {"make_state_across_lines", test_make_state_across_lines},
  {"make_is_rule", test_make_is_rule},
  {NULL, NULL},
};
