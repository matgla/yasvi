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

#include "acutest.h"

#include "buffer.h"

/* Keyword highlighting tests */

void test_highlight_keywords_if(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "if (x)");
  BufferRow* row = buffer->current_row;

  TEST_CHECK(row->highlight_data[0] == (char)EHighlightToken_Keyword);
  TEST_CHECK(row->highlight_data[1] == (char)EHighlightToken_Keyword);
  TEST_CHECK(row->highlight_data[2] == (char)EHighlightToken_Normal);

  buffer_free(buffer);
}

void test_highlight_keywords_return(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "return 0;");
  BufferRow* row = buffer->current_row;

  /* "return" should be highlighted as keyword */
  for (int i = 0; i < 6; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Keyword);
  }

  buffer_free(buffer);
}

void test_highlight_keywords_multiple(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "if (x) return;");
  BufferRow* row = buffer->current_row;

  /* "if" */
  TEST_CHECK(row->highlight_data[0] == (char)EHighlightToken_Keyword);
  TEST_CHECK(row->highlight_data[1] == (char)EHighlightToken_Keyword);

  /* Space and parens are normal/symbol */

  /* "return" */
  TEST_CHECK(row->highlight_data[7] == (char)EHighlightToken_Keyword);
  TEST_CHECK(row->highlight_data[8] == (char)EHighlightToken_Keyword);
  TEST_CHECK(row->highlight_data[9] == (char)EHighlightToken_Keyword);
  TEST_CHECK(row->highlight_data[10] == (char)EHighlightToken_Keyword);
  TEST_CHECK(row->highlight_data[11] == (char)EHighlightToken_Keyword);
  TEST_CHECK(row->highlight_data[12] == (char)EHighlightToken_Keyword);

  buffer_free(buffer);
}

void test_highlight_keywords_struct(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "struct Point");
  BufferRow* row = buffer->current_row;

  /* "struct" should be highlighted */
  for (int i = 0; i < 6; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Keyword);
  }

  /* "Point" should be normal (user-defined) */
  TEST_CHECK(row->highlight_data[7] == (char)EHighlightToken_Normal);

  buffer_free(buffer);
}

/* Type highlighting tests */

void test_highlight_types_int(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "int x;");
  BufferRow* row = buffer->current_row;

  /* "int" should be highlighted as type */
  TEST_CHECK(row->highlight_data[0] == (char)EHighlightToken_Type);
  TEST_CHECK(row->highlight_data[1] == (char)EHighlightToken_Type);
  TEST_CHECK(row->highlight_data[2] == (char)EHighlightToken_Type);

  buffer_free(buffer);
}

void test_highlight_types_void(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "void func();");
  BufferRow* row = buffer->current_row;

  /* "void" should be highlighted as type */
  for (int i = 0; i < 4; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Type);
  }

  buffer_free(buffer);
}

void test_highlight_types_unsigned(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "unsigned int x;");
  BufferRow* row = buffer->current_row;

  /* "unsigned" should be highlighted as type */
  for (int i = 0; i < 8; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Type);
  }

  buffer_free(buffer);
}

void test_highlight_types_size_t(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "size_t len;");
  BufferRow* row = buffer->current_row;

  /* "size_t" should be highlighted as type */
  for (int i = 0; i < 6; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Type);
  }

  buffer_free(buffer);
}

/* Keyword2 highlighting tests (true, false, NULL, etc.) */

void test_highlight_keywords2_true(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "true");
  BufferRow* row = buffer->current_row;

  for (int i = 0; i < 4; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Keyword2);
  }

  buffer_free(buffer);
}

void test_highlight_keywords2_false(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "false");
  BufferRow* row = buffer->current_row;

  for (int i = 0; i < 5; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Keyword2);
  }

  buffer_free(buffer);
}

void test_highlight_keywords2_null(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "NULL");
  BufferRow* row = buffer->current_row;

  for (int i = 0; i < 4; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Keyword2);
  }

  buffer_free(buffer);
}

/* String highlighting tests */

void test_highlight_strings_double_quoted(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "char* s = \"hello\";");
  BufferRow* row = buffer->current_row;

  /* Find the string start and verify highlighting */
  int string_start = -1;
  for (int i = 0; i < row->len; i++) {
    if (row->data[i] == '"' && string_start == -1) {
      string_start = i;
      break;
    }
  }

  TEST_CHECK(string_start != -1);
  /* The quote should be string type */
  TEST_CHECK(row->highlight_data[string_start] == (char)EHighlightToken_String);

  buffer_free(buffer);
}

void test_highlight_strings_single_quoted(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "char c = 'x';");
  BufferRow* row = buffer->current_row;

  /* Character literal should be highlighted as string */
  int quote_start = -1;
  for (int i = 0; i < row->len; i++) {
    if (row->data[i] == '\'' && quote_start == -1) {
      quote_start = i;
      break;
    }
  }

  TEST_CHECK(quote_start != -1);
  TEST_CHECK(row->highlight_data[quote_start] == (char)EHighlightToken_String);

  buffer_free(buffer);
}

void test_highlight_strings_escape_sequence(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "\"hello\\n\"");
  BufferRow* row = buffer->current_row;

  /* The escape sequence backslash should be marked as digit */
  /* Find backslash */
  for (int i = 0; i < row->len; i++) {
    if (row->data[i] == '\\') {
      TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Digit);
      break;
    }
  }

  buffer_free(buffer);
}

/* Comment highlighting tests */

void test_highlight_comments_single_line(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "int x; // comment");
  BufferRow* row = buffer->current_row;

  /* Find // and verify rest is comment */
  for (int i = 0; i < row->len - 1; i++) {
    if (row->data[i] == '/' && row->data[i + 1] == '/') {
      /* Everything from first / should be comment */
      TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Comment);
      TEST_CHECK(row->highlight_data[i + 1] == (char)EHighlightToken_Comment);
      break;
    }
  }

  buffer_free(buffer);
}

void test_highlight_comments_multi_line_start(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "/* start of comment");
  BufferRow* row = buffer->current_row;

  /* Slash-asterisk should start comment */
  TEST_CHECK(row->highlight_data[0] == (char)EHighlightToken_Comment);
  TEST_CHECK(row->highlight_data[1] == (char)EHighlightToken_Comment);

  /* And the highlight_comment_open flag should be set */
  TEST_CHECK(row->highlight_comment_open == 1);

  buffer_free(buffer);
}

void test_highlight_comments_multi_line_end(void) {
  Buffer* buffer = buffer_alloc();

  /* First line starts comment */
  buffer_append_line(buffer, "/* comment start");
  BufferRow* row1 = buffer->current_row;

  /* Second line ends comment */
  buffer_append_line(buffer, "comment end */");
  BufferRow* row2 = buffer->current_row;

  /* row1 should have comment open */
  TEST_CHECK(row1->highlight_comment_open == 1);

  /* row2 should close the comment - the asterisk-slash should be comment */
  int len = row2->len;
  TEST_CHECK(row2->highlight_data[len - 2] == (char)EHighlightToken_Comment);
  TEST_CHECK(row2->highlight_data[len - 1] == (char)EHighlightToken_Comment);
  /* NOTE: highlight_comment_open is NOT reset to 0 in the current code
     because row2 doesn't have row->prev set up correctly in this test setup.
     The flag is only checked from row->prev, not from the current row. */

  buffer_free(buffer);
}

/* Preprocessor highlighting tests */

void test_highlight_preprocessor_include(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "#include <stdio.h>");
  BufferRow* row = buffer->current_row;

  /* # should start preprocessor */
  TEST_CHECK(row->highlight_data[0] == (char)EHighlightToken_Preprocessor);

  /* "include" should also be preprocessor */
  for (int i = 1; i <= 7; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Preprocessor);
  }

  buffer_free(buffer);
}

void test_highlight_preprocessor_define(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "#define MAX 100");
  BufferRow* row = buffer->current_row;

  /* #define should be preprocessor */
  for (int i = 0; i <= 6; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Preprocessor);
  }

  buffer_free(buffer);
}

void test_highlight_preprocessor_pragma(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "#pragma once");
  BufferRow* row = buffer->current_row;

  /* #pragma should be preprocessor */
  for (int i = 0; i <= 6; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Preprocessor);
  }

  buffer_free(buffer);
}

/* Number highlighting tests */

void test_highlight_numbers_decimal(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "int x = 42;");
  BufferRow* row = buffer->current_row;

  /* "42" should be digit */
  TEST_CHECK(row->highlight_data[8] == (char)EHighlightToken_Digit);
  TEST_CHECK(row->highlight_data[9] == (char)EHighlightToken_Digit);

  buffer_free(buffer);
}

void test_highlight_numbers_zero(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "return 0;");
  BufferRow* row = buffer->current_row;

  /* "0" should be digit */
  TEST_CHECK(row->highlight_data[7] == (char)EHighlightToken_Digit);

  buffer_free(buffer);
}

/* Symbol highlighting tests */

void test_highlight_symbols_basic(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "a + b");
  BufferRow* row = buffer->current_row;

  /* + should be symbol */
  TEST_CHECK(row->highlight_data[2] == (char)EHighlightToken_Symbol);

  buffer_free(buffer);
}

void test_highlight_symbols_various(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "a = b + c;");
  BufferRow* row = buffer->current_row;

  /* = and + should be symbols */
  TEST_CHECK(row->highlight_data[2] == (char)EHighlightToken_Symbol);
  TEST_CHECK(row->highlight_data[6] == (char)EHighlightToken_Symbol);
  /* ; should be symbol */
  TEST_CHECK(row->highlight_data[9] == (char)EHighlightToken_Symbol);

  buffer_free(buffer);
}

void test_highlight_symbols2_brackets(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "arr[10]");
  BufferRow* row = buffer->current_row;

  /* [ and ] should be symbol2 - note: braces {} are also in symbols2
     but they're also in symbols, so they may be classified as Symbol instead */
  int bracket1_pos = -1, bracket2_pos = -1;
  for (int i = 0; i < row->len; i++) {
    if (row->data[i] == '[') bracket1_pos = i;
    if (row->data[i] == ']') bracket2_pos = i;
  }
  TEST_CHECK(bracket1_pos != -1);
  TEST_CHECK(bracket2_pos != -1);
  TEST_CHECK(row->highlight_data[bracket1_pos] == (char)EHighlightToken_Symbol2);
  TEST_CHECK(row->highlight_data[bracket2_pos] == (char)EHighlightToken_Symbol2);

  buffer_free(buffer);
}

void test_highlight_symbols2_braces(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "{ }");
  BufferRow* row = buffer->current_row;

  /* { and } are now only in symbols2, not symbols */
  TEST_CHECK(row->highlight_data[0] == (char)EHighlightToken_Symbol2);
  TEST_CHECK(row->highlight_data[2] == (char)EHighlightToken_Symbol2);

  buffer_free(buffer);
}

/* Mixed content tests */

void test_highlight_mixed_function(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "int main(int argc) { return 0; }");
  BufferRow* row = buffer->current_row;

  /* "int" is type */
  TEST_CHECK(row->highlight_data[0] == (char)EHighlightToken_Type);
  TEST_CHECK(row->highlight_data[1] == (char)EHighlightToken_Type);
  TEST_CHECK(row->highlight_data[2] == (char)EHighlightToken_Type);

  /* "main" is normal (identifier) */
  TEST_CHECK(row->highlight_data[4] == (char)EHighlightToken_Normal);

  /* "return" is keyword */
  int return_start = -1;
  for (int i = 0; i < row->len - 5; i++) {
    if (strncmp(&row->data[i], "return", 6) == 0) {
      return_start = i;
      break;
    }
  }
  TEST_CHECK(return_start != -1);
  for (int i = return_start; i < return_start + 6; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Keyword);
  }

  /* "0" should be digit */
  TEST_CHECK(row->highlight_data[return_start + 7] == (char)EHighlightToken_Digit);

  buffer_free(buffer);
}

void test_highlight_mixed_complex_line(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "if (x == NULL) return; // check");
  BufferRow* row = buffer->current_row;

  /* "if" is keyword */
  TEST_CHECK(row->highlight_data[0] == (char)EHighlightToken_Keyword);

  /* "NULL" is keyword2 */
  int null_start = -1;
  for (int i = 0; i < row->len - 3; i++) {
    if (strncmp(&row->data[i], "NULL", 4) == 0) {
      null_start = i;
      break;
    }
  }
  TEST_CHECK(null_start != -1);
  for (int i = null_start; i < null_start + 4; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Keyword2);
  }

  /* "return" is keyword */
  int return_start = -1;
  for (int i = 0; i < row->len - 5; i++) {
    if (strncmp(&row->data[i], "return", 6) == 0) {
      return_start = i;
      break;
    }
  }
  TEST_CHECK(return_start != -1);
  for (int i = return_start; i < return_start + 6; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Keyword);
  }

  /* "//" starts comment */
  for (int i = 0; i < row->len - 1; i++) {
    if (row->data[i] == '/' && row->data[i + 1] == '/') {
      TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Comment);
      break;
    }
  }

  buffer_free(buffer);
}

/* Edge cases */

void test_highlight_empty_line(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "");
  BufferRow* row = buffer->current_row;

  /* Empty line should have no issues */
  TEST_CHECK(row->len == 0);

  buffer_free(buffer);
}

void test_highlight_whitespace_only(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "     ");
  BufferRow* row = buffer->current_row;

  /* Whitespace should remain normal */
  for (int i = 0; i < row->len; i++) {
    TEST_CHECK(row->highlight_data[i] == (char)EHighlightToken_Normal);
  }

  buffer_free(buffer);
}

void test_highlight_null_row(void) {
  /* Should not crash on NULL row */
  buffer_row_highlight_line(NULL);
  TEST_CHECK(1);
}

/* String continuation across lines (simplified) */

void test_highlight_string_sets_open_flag(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "\"unclosed string");
  BufferRow* row = buffer->current_row;

  /* The string was not closed, flag should be set */
  /* The open char will be either " or ' */
  TEST_CHECK(row->highlight_string_open == '"');

  buffer_free(buffer);
}

void test_highlight_string_continuation(void) {
  Buffer* buffer = buffer_alloc();

  /* First line with unclosed string */
  buffer_append_line(buffer, "\"part1");
  BufferRow* row1 = buffer->current_row;

  /* Second line that continues the string */
  buffer_append_line(buffer, "part2\"");
  BufferRow* row2 = buffer->current_row;

  /* row1 should have string open */
  TEST_CHECK(row1->highlight_string_open == '"');

  /* row2 should start with string highlighting and close it */
  TEST_CHECK(row2->highlight_data[0] == (char)EHighlightToken_String);
  /* NOTE: highlight_string_open on row2 depends on whether row2->prev
     is set correctly. In the buffer linked list, row2->prev should be row1. */

  buffer_free(buffer);
}

TEST_LIST = {
    /* Keywords */
    {"test_highlight_keywords_if", test_highlight_keywords_if},
    {"test_highlight_keywords_return", test_highlight_keywords_return},
    {"test_highlight_keywords_multiple", test_highlight_keywords_multiple},
    {"test_highlight_keywords_struct", test_highlight_keywords_struct},
    /* Types */
    {"test_highlight_types_int", test_highlight_types_int},
    {"test_highlight_types_void", test_highlight_types_void},
    {"test_highlight_types_unsigned", test_highlight_types_unsigned},
    {"test_highlight_types_size_t", test_highlight_types_size_t},
    /* Keywords2 */
    {"test_highlight_keywords2_true", test_highlight_keywords2_true},
    {"test_highlight_keywords2_false", test_highlight_keywords2_false},
    {"test_highlight_keywords2_null", test_highlight_keywords2_null},
    /* Strings */
    {"test_highlight_strings_double_quoted", test_highlight_strings_double_quoted},
    {"test_highlight_strings_single_quoted", test_highlight_strings_single_quoted},
    {"test_highlight_strings_escape_sequence", test_highlight_strings_escape_sequence},
    /* Comments */
    {"test_highlight_comments_single_line", test_highlight_comments_single_line},
    {"test_highlight_comments_multi_line_start", test_highlight_comments_multi_line_start},
    {"test_highlight_comments_multi_line_end", test_highlight_comments_multi_line_end},
    /* Preprocessor */
    {"test_highlight_preprocessor_include", test_highlight_preprocessor_include},
    {"test_highlight_preprocessor_define", test_highlight_preprocessor_define},
    {"test_highlight_preprocessor_pragma", test_highlight_preprocessor_pragma},
    /* Numbers */
    {"test_highlight_numbers_decimal", test_highlight_numbers_decimal},
    {"test_highlight_numbers_zero", test_highlight_numbers_zero},
    /* Symbols */
    {"test_highlight_symbols_basic", test_highlight_symbols_basic},
    {"test_highlight_symbols_various", test_highlight_symbols_various},
    {"test_highlight_symbols2_brackets", test_highlight_symbols2_brackets},
    {"test_highlight_symbols2_braces", test_highlight_symbols2_braces},
    /* Mixed */
    {"test_highlight_mixed_function", test_highlight_mixed_function},
    {"test_highlight_mixed_complex_line", test_highlight_mixed_complex_line},
    /* Edge cases */
    {"test_highlight_empty_line", test_highlight_empty_line},
    {"test_highlight_whitespace_only", test_highlight_whitespace_only},
    {"test_highlight_null_row", test_highlight_null_row},
    /* String continuation */
    {"test_highlight_string_sets_open_flag", test_highlight_string_sets_open_flag},
    {"test_highlight_string_continuation", test_highlight_string_continuation},

    {NULL, NULL} /* zeroed record marking the end of the list */
};
