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

// The yasc dialect in the C highlighter. yasc is tcc's default dialect, so its
// words are keywords in any C file -- but only where tcc's parser would take
// them as keywords; elsewhere they are names. The same cases as
// tools/vscode-yasc/test/run.js.

#include "acutest.h"

#include "buffer.h"
#include "buffer_row.h"
#include "filetypes/c.h"

#include <string.h>

// A C buffer of the given lines (NULL-terminated), highlighted top-down.
static Buffer* c_buffer(const char* const* lines) {
  Buffer* buffer = buffer_alloc();
  buffer->filetype = &g_filetype_c;
  for (const char* const* line = lines; *line != NULL; ++line) {
    buffer_append_line(buffer, *line);
  }
  return buffer;
}

static BufferRow* row_at(const Buffer* buffer, int index) {
  BufferRow* row = buffer_get_first_row(buffer);
  while (row != NULL && index-- > 0) {
    row = row->next;
  }
  TEST_ASSERT(row != NULL);
  return row;
}

// The token of the first word of `needle`, found at its first occurrence in
// the row (the rest of `needle` only picks the occurrence); all of that word
// must carry the same token.
static int token_of(const BufferRow* row, const char* needle) {
  const char* at = strstr(row->data, needle);
  TEST_ASSERT_(at != NULL, "'%s' not in '%s'", needle, row->data);
  const int start = (int)(at - row->data);
  const int word_len = (int)strspn(needle,
                                   "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789");
  const int token = row->highlight_data[start];
  for (int i = start; i < start + word_len; ++i) {
    TEST_CHECK_(row->highlight_data[i] == token, "'%.*s' in '%s' is not one token", word_len,
                needle, row->data);
  }
  return token;
}

#define K EHighlightToken_Keyword
#define K2 EHighlightToken_Keyword2
#define T EHighlightToken_Type
#define N EHighlightToken_Normal

void test_yasc_ownership_qualifiers(void) {
  const char* lines[] = {
    "char * owned dup(const char * view s);",
    "void fill(out int * view p, drop char * owned q, int * view maybe m)",
    "  char * owned * view argv = 0;",
    "  struct name * owned maybe next;",
    "  int f(char * view, int *view);",
    "int main(int argc, char * view * view argv) {",
    NULL};
  Buffer* buffer = c_buffer(lines);
  TEST_CHECK(token_of(row_at(buffer, 0), "owned") == K);
  TEST_CHECK(token_of(row_at(buffer, 0), "view") == K);
  TEST_CHECK(token_of(row_at(buffer, 0), "dup") == N);
  TEST_CHECK(token_of(row_at(buffer, 1), "out") == K);
  TEST_CHECK(token_of(row_at(buffer, 1), "view p") == K);
  TEST_CHECK(token_of(row_at(buffer, 1), "drop") == K);
  TEST_CHECK(token_of(row_at(buffer, 1), "owned q") == K);
  TEST_CHECK(token_of(row_at(buffer, 1), "maybe") == K);
  TEST_CHECK(token_of(row_at(buffer, 2), "owned") == K);
  TEST_CHECK(token_of(row_at(buffer, 2), "view") == K);
  TEST_CHECK(token_of(row_at(buffer, 3), "owned") == K);
  TEST_CHECK(token_of(row_at(buffer, 3), "maybe") == K);
  TEST_CHECK(token_of(row_at(buffer, 4), "view,") == K);
  TEST_CHECK(token_of(row_at(buffer, 4), "view)") == K);
  TEST_CHECK(token_of(row_at(buffer, 5), "view *") == K);
  TEST_CHECK(token_of(row_at(buffer, 5), "view argv") == K);
  buffer_free(buffer);
}

// In regular C positions, and in expressions, the words are names
void test_yasc_words_as_names(void) {
  const char* lines[] = {
    "int out;",
    "struct view *v; view *w;",
    "  out->len = s.view + p->maybe;",
    "  if (out != 0 && drop(x) || out + 1) return out ? 1 : 0;",
    "  int output = viewer + owned_count;",
    "  goto out;",
    "  x = a * view + b;",
    "f(a, view); drop = 1; maybe [2]; out : 3; owned, x;",
    "  /* owned view */ const char * view str = \"defer owned\";",
    "  if (!error.is_warning) error = report.error;",
    "  int error = 0; try = catch; module = import + export;",
    NULL};
  Buffer* buffer = c_buffer(lines);
  TEST_CHECK(token_of(row_at(buffer, 0), "out") == N);
  TEST_CHECK(token_of(row_at(buffer, 1), "view *v") == N);
  TEST_CHECK(token_of(row_at(buffer, 1), "view *w") == N);
  TEST_CHECK(token_of(row_at(buffer, 2), "out") == N);
  TEST_CHECK(token_of(row_at(buffer, 2), "view") == N);
  TEST_CHECK(token_of(row_at(buffer, 2), "maybe") == N);
  TEST_CHECK(token_of(row_at(buffer, 3), "out") == N);
  TEST_CHECK(token_of(row_at(buffer, 3), "drop") == N);
  TEST_CHECK(token_of(row_at(buffer, 3), "out ?") == N);
  TEST_CHECK(token_of(row_at(buffer, 4), "output") == N);
  TEST_CHECK(token_of(row_at(buffer, 4), "viewer") == N);
  TEST_CHECK(token_of(row_at(buffer, 4), "owned_count") == N);
  TEST_CHECK(token_of(row_at(buffer, 5), "out") == N);
  TEST_CHECK(token_of(row_at(buffer, 6), "view") == N);
  TEST_CHECK(token_of(row_at(buffer, 7), "view") == N);
  TEST_CHECK(token_of(row_at(buffer, 7), "drop") == N);
  TEST_CHECK(token_of(row_at(buffer, 7), "maybe") == N);
  TEST_CHECK(token_of(row_at(buffer, 7), "out :") == N);
  TEST_CHECK(token_of(row_at(buffer, 7), "owned") == N);
  TEST_CHECK(token_of(row_at(buffer, 8), "owned") == EHighlightToken_Comment);
  TEST_CHECK(token_of(row_at(buffer, 8), "view str") == K);
  TEST_CHECK(token_of(row_at(buffer, 8), "defer") == EHighlightToken_String);
  TEST_CHECK(token_of(row_at(buffer, 9), "error") == N);
  TEST_CHECK(token_of(row_at(buffer, 9), "is_warning") == N);
  TEST_CHECK(token_of(row_at(buffer, 9), "error =") == N);
  TEST_CHECK(token_of(row_at(buffer, 9), "error;") == N);
  TEST_CHECK(token_of(row_at(buffer, 10), "error") == N);
  TEST_CHECK(token_of(row_at(buffer, 10), "try") == N);
  TEST_CHECK(token_of(row_at(buffer, 10), "catch") == N);
  TEST_CHECK(token_of(row_at(buffer, 10), "module") == N);
  TEST_CHECK(token_of(row_at(buffer, 10), "import") == N);
  TEST_CHECK(token_of(row_at(buffer, 10), "export") == N);
  buffer_free(buffer);
}

void test_yasc_defer(void) {
  const char* lines[] = {
    "  defer free(q);",
    "  defer { note(\"x\"); }",
    "  errdefer mem_free(buf);",
    "  defer (void)close(fd);",
    "  defer",
    NULL};
  Buffer* buffer = c_buffer(lines);
  for (int i = 0; i < 5; ++i) {
    TEST_CHECK_(token_of(row_at(buffer, i), i == 2 ? "errdefer" : "defer") == K, "line %d", i);
  }
  buffer_free(buffer);
}

void test_yasc_errors(void) {
  const char* lines[] = {
    "error ParseError { Empty, BadDigit = 7, TooBig };",
    "error FsError",
    "{",
    "  FileNotFound = ENOENT, /* errno */ BadPath",
    "};",
    "static !int parse_int(const char * view s);",
    "!char * owned make_label(const char * view num);",
    "!vec2_t vec_parse(const char * view s);",
    "!struct stack * owned stack_new(int cap);",
    "static !int add(error ParseError pe, error any)",
    "  if (!ok) return error.Empty;",
    "  int v = try parse_int(a) + try parse_int(b);",
    "  int a = parse_int(\"42\") catch -1;",
    "  int b = parse_int(s) catch |e| {",
    "    printf(\"%s\", error_name(e));",
    "  x = error_from(errno); n->next = move(head); f(borrow(p), s.move(1));",
    "  if (!vec_ok(v) && n != 0) return !x;",
    "  return v",
    "    catch 0;",
    "int after = Empty;",
    NULL};
  Buffer* buffer = c_buffer(lines);
  TEST_CHECK(token_of(row_at(buffer, 0), "error") == K);
  TEST_CHECK(token_of(row_at(buffer, 0), "ParseError") == T);
  TEST_CHECK(token_of(row_at(buffer, 0), "Empty") == K2);
  TEST_CHECK(token_of(row_at(buffer, 0), "BadDigit") == K2);
  TEST_CHECK(token_of(row_at(buffer, 0), "7") == EHighlightToken_Digit);
  TEST_CHECK(token_of(row_at(buffer, 0), "TooBig") == K2);
  TEST_CHECK(token_of(row_at(buffer, 1), "error") == K);
  TEST_CHECK(token_of(row_at(buffer, 1), "FsError") == T);
  TEST_CHECK(token_of(row_at(buffer, 3), "FileNotFound") == K2);
  TEST_CHECK(token_of(row_at(buffer, 3), "errno") == EHighlightToken_Comment);
  TEST_CHECK(token_of(row_at(buffer, 3), "BadPath") == K2);
  TEST_CHECK(token_of(row_at(buffer, 5), "!") == T);
  TEST_CHECK(token_of(row_at(buffer, 5), "int") == T);
  TEST_CHECK(token_of(row_at(buffer, 6), "!") == T);
  TEST_CHECK(token_of(row_at(buffer, 7), "!") == T);
  TEST_CHECK(token_of(row_at(buffer, 8), "!") == T);
  TEST_CHECK(token_of(row_at(buffer, 9), "!") == T);
  TEST_CHECK(token_of(row_at(buffer, 9), "error") == K);
  TEST_CHECK(token_of(row_at(buffer, 9), "ParseError") == T);
  TEST_CHECK(token_of(row_at(buffer, 9), "pe") == N);
  TEST_CHECK(token_of(row_at(buffer, 9), "error any") == K);
  TEST_CHECK(token_of(row_at(buffer, 9), "any") == N);
  TEST_CHECK(token_of(row_at(buffer, 10), "!") == EHighlightToken_Symbol);
  TEST_CHECK(token_of(row_at(buffer, 10), "error") == K);
  TEST_CHECK(token_of(row_at(buffer, 10), "Empty") == K2);
  TEST_CHECK(token_of(row_at(buffer, 11), "try") == K);
  TEST_CHECK(token_of(row_at(buffer, 11), "try parse_int(b)") == K);
  TEST_CHECK(token_of(row_at(buffer, 12), "catch") == K);
  TEST_CHECK(token_of(row_at(buffer, 13), "catch") == K);
  TEST_CHECK(token_of(row_at(buffer, 14), "error_name") == K);
  TEST_CHECK(token_of(row_at(buffer, 15), "error_from") == K);
  TEST_CHECK(token_of(row_at(buffer, 15), "move") == K);
  TEST_CHECK(token_of(row_at(buffer, 15), "borrow") == K);
  TEST_CHECK(token_of(row_at(buffer, 15), "move(1)") == N);
  TEST_CHECK(token_of(row_at(buffer, 16), "!") == EHighlightToken_Symbol);
  TEST_CHECK(token_of(row_at(buffer, 16), "!x") == EHighlightToken_Symbol);
  TEST_CHECK(token_of(row_at(buffer, 18), "catch") == K);
  TEST_CHECK(token_of(row_at(buffer, 19), "Empty") == N);  // the set is closed
  buffer_free(buffer);
}

void test_yasc_modules(void) {
  const char* lines[] = {
    "module lib.vec;",
    "import lib.shapes;",
    "export typedef struct vec2 { int x, y; } vec2_t;",
    "  export vec2_t vec_add(vec2_t a, vec2_t b);",
    "int f(void) { import x; }",
    NULL};
  Buffer* buffer = c_buffer(lines);
  TEST_CHECK(token_of(row_at(buffer, 0), "module") == K);
  TEST_CHECK(token_of(row_at(buffer, 1), "import") == K);
  TEST_CHECK(token_of(row_at(buffer, 2), "export") == K);
  TEST_CHECK(token_of(row_at(buffer, 3), "export") == K);
  TEST_CHECK(token_of(row_at(buffer, 4), "import") == N);
  buffer_free(buffer);
}

// Closing an error set that spans lines re-highlights the lines below it
void test_yasc_error_set_closed_later(void) {
  const char* lines[] = {"error E {", "  A, B", "", "int x = A;", NULL};
  Buffer* buffer = c_buffer(lines);
  TEST_CHECK(token_of(row_at(buffer, 3), "A;") == K2);
  buffer_row_insert_chars(row_at(buffer, 2), 0, "};", 2);
  TEST_CHECK(token_of(row_at(buffer, 1), "B") == K2);
  TEST_CHECK(token_of(row_at(buffer, 3), "A;") == N);
  buffer_free(buffer);
}

// Whole numbers, not a digit and then a name
void test_c_numbers(void) {
  const char* lines[] = {"x = 0x1F + 10u + 1.5e-3f + 0b101 + 1'000;", NULL};
  Buffer* buffer = c_buffer(lines);
  TEST_CHECK(token_of(row_at(buffer, 0), "0x1F") == EHighlightToken_Digit);
  TEST_CHECK(token_of(row_at(buffer, 0), "10u") == EHighlightToken_Digit);
  TEST_CHECK(token_of(row_at(buffer, 0), "1.5e") == EHighlightToken_Digit);
  TEST_CHECK(token_of(row_at(buffer, 0), "3f") == EHighlightToken_Digit);
  TEST_CHECK(token_of(row_at(buffer, 0), "0b101") == EHighlightToken_Digit);
  TEST_CHECK(token_of(row_at(buffer, 0), "000") == EHighlightToken_Digit);
  buffer_free(buffer);
}

void test_c_newer_keywords(void) {
  const char* lines[] = {"_Static_assert(sizeof(_Bool) == 1); static_assert(alignof(int) == 4);",
                         "typeof(x) y; constexpr int z = 1; __attribute__((noreturn)) void f(void);",
                         NULL};
  Buffer* buffer = c_buffer(lines);
  TEST_CHECK(token_of(row_at(buffer, 0), "_Static_assert") == K);
  TEST_CHECK(token_of(row_at(buffer, 0), "_Bool") == T);
  TEST_CHECK(token_of(row_at(buffer, 0), "static_assert") == K);
  TEST_CHECK(token_of(row_at(buffer, 0), "alignof") == K);
  TEST_CHECK(token_of(row_at(buffer, 1), "typeof") == K);
  TEST_CHECK(token_of(row_at(buffer, 1), "constexpr") == K);
  TEST_CHECK(token_of(row_at(buffer, 1), "__attribute__") == K);
  buffer_free(buffer);
}

TEST_LIST = {
    {"test_yasc_ownership_qualifiers", test_yasc_ownership_qualifiers},
    {"test_yasc_words_as_names", test_yasc_words_as_names},
    {"test_yasc_defer", test_yasc_defer},
    {"test_yasc_errors", test_yasc_errors},
    {"test_yasc_modules", test_yasc_modules},
    {"test_yasc_error_set_closed_later", test_yasc_error_set_closed_later},
    {"test_c_numbers", test_c_numbers},
    {"test_c_newer_keywords", test_c_newer_keywords},
    {NULL, NULL},
};
