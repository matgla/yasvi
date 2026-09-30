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

#include "acutest.h"

#include "buffer.h"
#include "filetypes/zig.h"

static Buffer* zig_buffer(const char* line) {
  Buffer* buffer = buffer_alloc();
  buffer->filetype = &g_filetype_zig;
  buffer_append_line(buffer, line);
  return buffer;
}

/* Checks that columns [start, end) all carry the expected token */
static void check_range(const BufferRow* row, int start, int end,
                        EHighlightToken token) {
  for (int i = start; i < end; ++i) {
    TEST_CHECK_(row->highlight_data[i] == (char)token,
                "column %d of '%s' is %d, expected %d", i, row->data,
                row->highlight_data[i], token);
  }
}

/* Detection */

void test_zig_detected_by_extension(void) {
  TEST_CHECK(filetype_by_extension("main.zig") == &g_filetype_zig);
  TEST_CHECK(filetype_by_extension("build.zig.zon") == &g_filetype_zig);
  TEST_CHECK(filetype_detect("src/kernel.zig", NULL) == &g_filetype_zig);
  TEST_CHECK(filetype_by_extension("main.c") != &g_filetype_zig);
}

/* Keywords, types and values */

void test_zig_keywords(void) {
  Buffer* buffer = zig_buffer("pub fn main() void {");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 3, EHighlightToken_Keyword);   /* pub */
  check_range(row, 4, 6, EHighlightToken_Keyword);   /* fn */
  check_range(row, 7, 11, EHighlightToken_Normal);   /* main */
  check_range(row, 11, 13, EHighlightToken_Symbol);  /* () */
  check_range(row, 14, 18, EHighlightToken_Type);    /* void */
  check_range(row, 19, 20, EHighlightToken_Symbol2); /* { */

  buffer_free(buffer);
}

void test_zig_keyword_prefix_is_not_keyword(void) {
  Buffer* buffer = zig_buffer("const constant = fnx;");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 5, EHighlightToken_Keyword);
  check_range(row, 6, 14, EHighlightToken_Normal);
  check_range(row, 17, 20, EHighlightToken_Normal);

  buffer_free(buffer);
}

void test_zig_primitive_types(void) {
  Buffer* buffer = zig_buffer("x: usize, y: u8, z: i32, w: c_int");
  BufferRow* row = buffer->current_row;

  check_range(row, 3, 8, EHighlightToken_Type);   /* usize */
  check_range(row, 13, 15, EHighlightToken_Type); /* u8 */
  check_range(row, 20, 23, EHighlightToken_Type); /* i32 */
  check_range(row, 28, 33, EHighlightToken_Type); /* c_int */

  buffer_free(buffer);
}

void test_zig_arbitrary_width_ints(void) {
  Buffer* buffer = zig_buffer("u3 i127 u ux1 u1x");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 2, EHighlightToken_Type);
  check_range(row, 3, 7, EHighlightToken_Type);
  check_range(row, 8, 9, EHighlightToken_Normal);
  check_range(row, 10, 13, EHighlightToken_Normal);
  check_range(row, 14, 17, EHighlightToken_Normal);

  buffer_free(buffer);
}

void test_zig_values(void) {
  Buffer* buffer = zig_buffer("true false null undefined");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 4, EHighlightToken_Keyword2);
  check_range(row, 5, 10, EHighlightToken_Keyword2);
  check_range(row, 11, 15, EHighlightToken_Keyword2);
  check_range(row, 16, 25, EHighlightToken_Keyword2);

  buffer_free(buffer);
}

/* Builtins */

void test_zig_builtin(void) {
  Buffer* buffer = zig_buffer("const std = @import(\"std\");");
  BufferRow* row = buffer->current_row;

  check_range(row, 12, 19, EHighlightToken_Preprocessor); /* @import */
  check_range(row, 19, 20, EHighlightToken_Symbol);       /* ( */
  check_range(row, 20, 25, EHighlightToken_String);       /* "std" */
  check_range(row, 25, 27, EHighlightToken_Symbol);       /* ); */

  buffer_free(buffer);
}

void test_zig_quoted_identifier(void) {
  Buffer* buffer = zig_buffer("@\"if\" = 1");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 1, EHighlightToken_Preprocessor);
  check_range(row, 1, 5, EHighlightToken_String);
  check_range(row, 8, 9, EHighlightToken_Digit);

  buffer_free(buffer);
}

/* Comments */

void test_zig_line_comment(void) {
  Buffer* buffer = zig_buffer("x = 1; // if const");
  BufferRow* row = buffer->current_row;

  check_range(row, 4, 5, EHighlightToken_Digit);
  check_range(row, 7, 18, EHighlightToken_Comment);

  buffer_free(buffer);
}

void test_zig_doc_comments(void) {
  Buffer* buffer = zig_buffer("/// doc");
  check_range(buffer->current_row, 0, 7, EHighlightToken_Comment);
  buffer_append_line(buffer, "//! top");
  check_range(buffer_get_row(buffer, 1), 0, 7, EHighlightToken_Comment);
  buffer_free(buffer);
}

void test_zig_division_is_not_comment(void) {
  Buffer* buffer = zig_buffer("a / b");
  BufferRow* row = buffer->current_row;

  check_range(row, 2, 3, EHighlightToken_Symbol);
  check_range(row, 4, 5, EHighlightToken_Normal);

  buffer_free(buffer);
}

/* Strings */

void test_zig_string_with_escape(void) {
  Buffer* buffer = zig_buffer("\"a\\n\" if");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 2, EHighlightToken_String);
  check_range(row, 2, 4, EHighlightToken_Digit);
  check_range(row, 4, 5, EHighlightToken_String);
  check_range(row, 6, 8, EHighlightToken_Keyword);

  buffer_free(buffer);
}

void test_zig_escaped_quote_does_not_end_string(void) {
  Buffer* buffer = zig_buffer("\"a\\\"b\" x");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 2, EHighlightToken_String);
  check_range(row, 2, 4, EHighlightToken_Digit);
  check_range(row, 4, 6, EHighlightToken_String);
  check_range(row, 7, 8, EHighlightToken_Normal);

  buffer_free(buffer);
}

void test_zig_char_literal(void) {
  Buffer* buffer = zig_buffer("c == 'a'");
  check_range(buffer->current_row, 5, 8, EHighlightToken_String);
  buffer_free(buffer);
}

void test_zig_multiline_string(void) {
  Buffer* buffer = zig_buffer("    \\\\ if \"x\" // y");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 4, EHighlightToken_Normal);
  check_range(row, 4, row->len, EHighlightToken_String);

  buffer_free(buffer);
}

void test_zig_unterminated_string_does_not_leak(void) {
  Buffer* buffer = zig_buffer("\"open");
  TEST_CHECK(buffer->current_row->highlight_string_open == 0);
  buffer_append_line(buffer, "const x");
  check_range(buffer_get_row(buffer, 1), 0, 5, EHighlightToken_Keyword);
  buffer_free(buffer);
}

void test_zig_c_block_comment_does_not_open(void) {
  Buffer* buffer = zig_buffer("a /* b");
  TEST_CHECK(buffer->current_row->highlight_comment_open == 0);
  buffer_append_line(buffer, "const x");
  check_range(buffer_get_row(buffer, 1), 0, 5, EHighlightToken_Keyword);
  buffer_free(buffer);
}

/* Numbers */

void test_zig_numbers(void) {
  Buffer* buffer = zig_buffer("0xff_ff 1_000 0b1010 0o17 1.5e-3 0x1p+4");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 7, EHighlightToken_Digit);
  check_range(row, 8, 13, EHighlightToken_Digit);
  check_range(row, 14, 20, EHighlightToken_Digit);
  check_range(row, 21, 25, EHighlightToken_Digit);
  check_range(row, 26, 32, EHighlightToken_Digit);
  check_range(row, 33, 39, EHighlightToken_Digit);

  buffer_free(buffer);
}

void test_zig_range_is_not_float(void) {
  Buffer* buffer = zig_buffer("0..5");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 1, EHighlightToken_Digit);
  check_range(row, 1, 3, EHighlightToken_Symbol);
  check_range(row, 3, 4, EHighlightToken_Digit);

  buffer_free(buffer);
}

void test_zig_hex_e_minus_is_subtraction(void) {
  Buffer* buffer = zig_buffer("0x1E-1");
  BufferRow* row = buffer->current_row;

  check_range(row, 0, 4, EHighlightToken_Digit);
  check_range(row, 4, 5, EHighlightToken_Symbol);
  check_range(row, 5, 6, EHighlightToken_Digit);

  buffer_free(buffer);
}

void test_zig_identifier_with_digits(void) {
  Buffer* buffer = zig_buffer("x2 = y");
  check_range(buffer->current_row, 0, 2, EHighlightToken_Normal);
  buffer_free(buffer);
}

TEST_LIST = {
    {"test_zig_detected_by_extension", test_zig_detected_by_extension},
    /* Words */
    {"test_zig_keywords", test_zig_keywords},
    {"test_zig_keyword_prefix_is_not_keyword",
     test_zig_keyword_prefix_is_not_keyword},
    {"test_zig_primitive_types", test_zig_primitive_types},
    {"test_zig_arbitrary_width_ints", test_zig_arbitrary_width_ints},
    {"test_zig_values", test_zig_values},
    {"test_zig_builtin", test_zig_builtin},
    {"test_zig_quoted_identifier", test_zig_quoted_identifier},
    /* Comments */
    {"test_zig_line_comment", test_zig_line_comment},
    {"test_zig_doc_comments", test_zig_doc_comments},
    {"test_zig_division_is_not_comment", test_zig_division_is_not_comment},
    /* Strings */
    {"test_zig_string_with_escape", test_zig_string_with_escape},
    {"test_zig_escaped_quote_does_not_end_string",
     test_zig_escaped_quote_does_not_end_string},
    {"test_zig_char_literal", test_zig_char_literal},
    {"test_zig_multiline_string", test_zig_multiline_string},
    {"test_zig_unterminated_string_does_not_leak",
     test_zig_unterminated_string_does_not_leak},
    {"test_zig_c_block_comment_does_not_open",
     test_zig_c_block_comment_does_not_open},
    /* Numbers */
    {"test_zig_numbers", test_zig_numbers},
    {"test_zig_range_is_not_float", test_zig_range_is_not_float},
    {"test_zig_hex_e_minus_is_subtraction", test_zig_hex_e_minus_is_subtraction},
    {"test_zig_identifier_with_digits", test_zig_identifier_with_digits},

    {NULL, NULL} /* zeroed record marking the end of the list */
};
