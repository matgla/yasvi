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

/* Replace line tests */

void test_buffer_row_replace_line_same_length(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_replace_line(row, "Goodbye all");
  TEST_CHECK(strcmp(row->data, "Goodbye all") == 0);
  TEST_CHECK(row->len == 11);

  buffer_free(buffer);
}

void test_buffer_row_replace_line_shorter(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_replace_line(row, "Hi");
  TEST_CHECK(strcmp(row->data, "Hi") == 0);
  TEST_CHECK(row->len == 2);

  buffer_free(buffer);
}

void test_buffer_row_replace_line_longer(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hi");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_replace_line(row, "Hello wonderful world");
  TEST_CHECK(strcmp(row->data, "Hello wonderful world") == 0);
  TEST_CHECK(row->len == 21);

  buffer_free(buffer);
}

void test_buffer_row_replace_line_empty(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_replace_line(row, "");
  TEST_CHECK(strcmp(row->data, "") == 0);
  TEST_CHECK(row->len == 0);

  buffer_free(buffer);
}

void test_buffer_row_replace_line_null(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Should not crash on NULL row or NULL line */
  buffer_row_replace_line(NULL, "test");
  buffer_row_replace_line(row, NULL);

  /* Data should be unchanged */
  TEST_CHECK(strcmp(row->data, "Hello") == 0);

  buffer_free(buffer);
}

/* Append string tests */

void test_buffer_row_append_str(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_append_str(row, " world", 6);
  TEST_CHECK(strcmp(row->data, "Hello world") == 0);
  TEST_CHECK(row->len == 11);

  buffer_free(buffer);
}

void test_buffer_row_append_str_partial(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Append only first 3 chars of string */
  buffer_row_append_str(row, " world", 3);
  TEST_CHECK(strcmp(row->data, "Hello wo") == 0);
  TEST_CHECK(row->len == 8);

  buffer_free(buffer);
}

void test_buffer_row_append_str_empty(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Append empty string */
  buffer_row_append_str(row, "", 0);
  TEST_CHECK(strcmp(row->data, "Hello") == 0);
  TEST_CHECK(row->len == 5);

  buffer_free(buffer);
}

void test_buffer_row_append_str_null_row(void) {
  /* Should not crash on NULL row */
  buffer_row_append_str(NULL, "test", 4);
  TEST_CHECK(1);
}

void test_buffer_row_append_str_reallocation(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  char big_string[200];
  memset(big_string, 'x', 199);
  big_string[199] = '\0';

  buffer_row_append_str(row, big_string, 199);
  TEST_CHECK(row->len == 199);
  TEST_CHECK(row->allocated_size >= 200);

  buffer_free(buffer);
}

/* Trim tests */

void test_buffer_row_trim_at_start(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Trim at index 0 - should result in empty string */
  buffer_row_trim(row, 0);
  TEST_CHECK(strcmp(row->data, "") == 0);
  TEST_CHECK(row->len == 0);

  buffer_free(buffer);
}

void test_buffer_row_trim_at_middle(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Trim after "Hello" */
  buffer_row_trim(row, 5);
  TEST_CHECK(strcmp(row->data, "Hello") == 0);
  TEST_CHECK(row->len == 5);

  buffer_free(buffer);
}

void test_buffer_row_trim_at_end(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Trim at end - should not change anything */
  buffer_row_trim(row, 11);
  TEST_CHECK(strcmp(row->data, "Hello world") == 0);
  TEST_CHECK(row->len == 11);

  buffer_free(buffer);
}

void test_buffer_row_trim_invalid(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Should not crash on invalid inputs */
  buffer_row_trim(NULL, 0);
  buffer_row_trim(row, -1);
  buffer_row_trim(row, 100);

  /* Data should be unchanged */
  TEST_CHECK(strcmp(row->data, "Hello") == 0);
  TEST_CHECK(row->len == 5);

  buffer_free(buffer);
}

/* Get length tests */

void test_buffer_row_get_length_normal(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int len = buffer_row_get_length(row);
  TEST_CHECK(len == 11);

  buffer_free(buffer);
}

void test_buffer_row_get_length_empty(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int len = buffer_row_get_length(row);
  TEST_CHECK(len == 0);

  buffer_free(buffer);
}

void test_buffer_row_get_length_null(void) {
  int len = buffer_row_get_length(NULL);
  TEST_CHECK(len == 0);
}

void test_buffer_row_get_length_after_modification(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hi");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  TEST_CHECK(buffer_row_get_length(row) == 2);

  buffer_row_append_char(row, '!');
  TEST_CHECK(buffer_row_get_length(row) == 3);

  buffer_row_remove_char(row, 0);
  TEST_CHECK(buffer_row_get_length(row) == 2);

  buffer_free(buffer);
}

/* Whitespace check tests */

void test_buffer_row_has_whitespace_at_position_space(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* ' ' at index 5 */
  TEST_CHECK(buffer_row_has_whitespace_at_position(row, 5) == true);
  /* 'H' at index 0 */
  TEST_CHECK(buffer_row_has_whitespace_at_position(row, 0) == false);

  buffer_free(buffer);
}

void test_buffer_row_has_whitespace_at_position_various(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  /* Test with various whitespace characters */
  buffer_append_line(buffer, "a b\tc");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* 'a' */
  TEST_CHECK(buffer_row_has_whitespace_at_position(row, 0) == false);
  /* ' ' */
  TEST_CHECK(buffer_row_has_whitespace_at_position(row, 1) == true);
  /* 'b' */
  TEST_CHECK(buffer_row_has_whitespace_at_position(row, 2) == false);
  /* '\t' */
  TEST_CHECK(buffer_row_has_whitespace_at_position(row, 3) == true);

  buffer_free(buffer);
}

void test_buffer_row_has_whitespace_at_position_invalid(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Should return false for invalid positions */
  TEST_CHECK(buffer_row_has_whitespace_at_position(NULL, 0) == false);
  TEST_CHECK(buffer_row_has_whitespace_at_position(row, -1) == false);
  TEST_CHECK(buffer_row_has_whitespace_at_position(row, 100) == false);

  buffer_free(buffer);
}

/* First char offset tests */

void test_buffer_row_get_offset_to_first_char_normal(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "   Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int offset = buffer_row_get_offset_to_first_char(row, 0);
  TEST_CHECK(offset == 3);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_first_char_no_whitespace(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int offset = buffer_row_get_offset_to_first_char(row, 0);
  TEST_CHECK(offset == 0);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_first_char_all_whitespace(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "     ");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int offset = buffer_row_get_offset_to_first_char(row, 0);
  TEST_CHECK(offset == 5); /* Returns length if all whitespace */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_first_char_from_middle(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello   world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int offset = buffer_row_get_offset_to_first_char(row, 5);
  TEST_CHECK(offset == 3); /* Skips the 3 spaces */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_first_char_invalid(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Should return 0 for invalid inputs */
  TEST_CHECK(buffer_row_get_offset_to_first_char(NULL, 0) == 0);
  TEST_CHECK(buffer_row_get_offset_to_first_char(row, -1) == 0);
  TEST_CHECK(buffer_row_get_offset_to_first_char(row, 100) == 0);

  buffer_free(buffer);
}

TEST_LIST = {
    {"test_buffer_row_replace_line_same_length",
     test_buffer_row_replace_line_same_length},
    {"test_buffer_row_replace_line_shorter", test_buffer_row_replace_line_shorter},
    {"test_buffer_row_replace_line_longer", test_buffer_row_replace_line_longer},
    {"test_buffer_row_replace_line_empty", test_buffer_row_replace_line_empty},
    {"test_buffer_row_replace_line_null", test_buffer_row_replace_line_null},
    {"test_buffer_row_append_str", test_buffer_row_append_str},
    {"test_buffer_row_append_str_partial", test_buffer_row_append_str_partial},
    {"test_buffer_row_append_str_empty", test_buffer_row_append_str_empty},
    {"test_buffer_row_append_str_null_row", test_buffer_row_append_str_null_row},
    {"test_buffer_row_append_str_reallocation",
     test_buffer_row_append_str_reallocation},
    {"test_buffer_row_trim_at_start", test_buffer_row_trim_at_start},
    {"test_buffer_row_trim_at_middle", test_buffer_row_trim_at_middle},
    {"test_buffer_row_trim_at_end", test_buffer_row_trim_at_end},
    {"test_buffer_row_trim_invalid", test_buffer_row_trim_invalid},
    {"test_buffer_row_get_length_normal", test_buffer_row_get_length_normal},
    {"test_buffer_row_get_length_empty", test_buffer_row_get_length_empty},
    {"test_buffer_row_get_length_null", test_buffer_row_get_length_null},
    {"test_buffer_row_get_length_after_modification",
     test_buffer_row_get_length_after_modification},
    {"test_buffer_row_has_whitespace_at_position_space",
     test_buffer_row_has_whitespace_at_position_space},
    {"test_buffer_row_has_whitespace_at_position_various",
     test_buffer_row_has_whitespace_at_position_various},
    {"test_buffer_row_has_whitespace_at_position_invalid",
     test_buffer_row_has_whitespace_at_position_invalid},
    {"test_buffer_row_get_offset_to_first_char_normal",
     test_buffer_row_get_offset_to_first_char_normal},
    {"test_buffer_row_get_offset_to_first_char_no_whitespace",
     test_buffer_row_get_offset_to_first_char_no_whitespace},
    {"test_buffer_row_get_offset_to_first_char_all_whitespace",
     test_buffer_row_get_offset_to_first_char_all_whitespace},
    {"test_buffer_row_get_offset_to_first_char_from_middle",
     test_buffer_row_get_offset_to_first_char_from_middle},
    {"test_buffer_row_get_offset_to_first_char_invalid",
     test_buffer_row_get_offset_to_first_char_invalid},

    {NULL, NULL} /* zeroed record marking the end of the list */
};
