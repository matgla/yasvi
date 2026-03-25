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

/* Insert character tests */

void test_buffer_row_insert_char_at_start(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_insert_char(row, 0, 'X');
  TEST_CHECK(strcmp(row->data, "XHello world") == 0);
  TEST_CHECK(row->len == 12);

  buffer_free(buffer);
}

void test_buffer_row_insert_char_at_end(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_insert_char(row, row->len, 'X');
  TEST_CHECK(strcmp(row->data, "Hello worldX") == 0);
  TEST_CHECK(row->len == 12);

  buffer_free(buffer);
}

void test_buffer_row_insert_char_at_middle(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_insert_char(row, 5, 'X');
  TEST_CHECK(strcmp(row->data, "HelloX world") == 0);
  TEST_CHECK(row->len == 12);

  buffer_free(buffer);
}

void test_buffer_row_insert_char_multiple(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "abc");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_insert_char(row, 1, 'X');
  buffer_row_insert_char(row, 2, 'Y');
  buffer_row_insert_char(row, 3, 'Z');
  TEST_CHECK(strcmp(row->data, "aXYZbc") == 0);
  TEST_CHECK(row->len == 6);

  buffer_free(buffer);
}

void test_buffer_row_insert_char_reallocation(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  /* Create a line that will trigger reallocation */
  char long_line[256];
  memset(long_line, 'a', 255);
  long_line[255] = '\0';

  buffer_append_line(buffer, long_line);
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Keep inserting until reallocation happens */
  for (int i = 0; i < 100; i++) {
    buffer_row_insert_char(row, 0, 'X');
  }

  TEST_CHECK(row->len == 355);
  TEST_CHECK(row->allocated_size >= row->len + 1);

  buffer_free(buffer);
}

/* Remove character tests */

void test_buffer_row_remove_char_at_start(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_remove_char(row, 0);
  TEST_CHECK(strcmp(row->data, "ello world") == 0);
  TEST_CHECK(row->len == 10);

  buffer_free(buffer);
}

void test_buffer_row_remove_char_at_end(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_remove_char(row, 10);
  TEST_CHECK(strcmp(row->data, "Hello worl") == 0);
  TEST_CHECK(row->len == 10);

  buffer_free(buffer);
}

void test_buffer_row_remove_char_at_middle(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_remove_char(row, 5);
  TEST_CHECK(strcmp(row->data, "Helloworld") == 0);
  TEST_CHECK(row->len == 10);

  buffer_free(buffer);
}

void test_buffer_row_remove_char_until_empty(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "ab");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_remove_char(row, 0);
  TEST_CHECK(strcmp(row->data, "b") == 0);
  TEST_CHECK(row->len == 1);

  buffer_row_remove_char(row, 0);
  TEST_CHECK(strcmp(row->data, "") == 0);
  TEST_CHECK(row->len == 0);

  buffer_free(buffer);
}

void test_buffer_row_remove_char_invalid_index(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Should handle invalid indices gracefully */
  bool result = buffer_row_remove_char(row, -1);
  TEST_CHECK(result == false);

  result = buffer_row_remove_char(row, 100);
  TEST_CHECK(result == false);

  /* Data should be unchanged */
  TEST_CHECK(strcmp(row->data, "Hello") == 0);
  TEST_CHECK(row->len == 5);

  buffer_free(buffer);
}

/* Append character tests */

void test_buffer_row_append_char(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_append_char(row, ' ');
  buffer_row_append_char(row, 'w');
  buffer_row_append_char(row, 'o');
  buffer_row_append_char(row, 'r');
  buffer_row_append_char(row, 'l');
  buffer_row_append_char(row, 'd');

  TEST_CHECK(strcmp(row->data, "Hello world") == 0);
  TEST_CHECK(row->len == 11);

  buffer_free(buffer);
}

void test_buffer_row_append_char_reallocation(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Append many characters to trigger reallocation */
  for (int i = 0; i < 200; i++) {
    buffer_row_append_char(row, 'a');
  }

  TEST_CHECK(row->len == 200);
  TEST_CHECK(row->allocated_size >= 201);

  buffer_free(buffer);
}

void test_buffer_row_append_char_null_row(void) {
  /* Should not crash on NULL row */
  buffer_row_append_char(NULL, 'X');
  /* If we get here without crash, test passes */
  TEST_CHECK(1);
}

/* Remove multiple characters tests */

void test_buffer_row_remove_chars_single(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int removed = buffer_row_remove_chars(row, 5, 1);
  TEST_CHECK(removed == 1);
  TEST_CHECK(strcmp(row->data, "Helloworld") == 0);

  buffer_free(buffer);
}

void test_buffer_row_remove_chars_multiple(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int removed = buffer_row_remove_chars(row, 0, 5);
  TEST_CHECK(removed == 5);
  TEST_CHECK(strcmp(row->data, " world") == 0);
  TEST_CHECK(row->len == 6);

  buffer_free(buffer);
}

void test_buffer_row_remove_chars_at_end(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int removed = buffer_row_remove_chars(row, 5, 6);
  TEST_CHECK(removed == 6);
  TEST_CHECK(strcmp(row->data, "Hello") == 0);
  TEST_CHECK(row->len == 5);

  buffer_free(buffer);
}

void test_buffer_row_remove_chars_beyond_end(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Try to remove more chars than available */
  int removed = buffer_row_remove_chars(row, 3, 10);
  TEST_CHECK(removed == 2); /* Only 2 chars available from index 3 */
  TEST_CHECK(strcmp(row->data, "Hel") == 0);
  TEST_CHECK(row->len == 3);

  buffer_free(buffer);
}

void test_buffer_row_remove_chars_invalid(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int removed = buffer_row_remove_chars(row, -1, 2);
  TEST_CHECK(removed == 0);

  removed = buffer_row_remove_chars(row, 100, 2);
  TEST_CHECK(removed == 0);

  removed = buffer_row_remove_chars(NULL, 0, 2);
  TEST_CHECK(removed == 0);

  buffer_free(buffer);
}

/* Insert multiple characters tests */

void test_buffer_row_insert_chars_single(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_insert_chars(row, 5, "X", 1);
  TEST_CHECK(strcmp(row->data, "HelloX world") == 0);
  TEST_CHECK(row->len == 12);

  buffer_free(buffer);
}

void test_buffer_row_insert_chars_multiple(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_insert_chars(row, 6, "beautiful ", 10);
  TEST_CHECK(strcmp(row->data, "Hello beautiful world") == 0);
  TEST_CHECK(row->len == 21);

  buffer_free(buffer);
}

void test_buffer_row_insert_chars_at_start(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_insert_chars(row, 0, "Hello ", 6);
  TEST_CHECK(strcmp(row->data, "Hello world") == 0);

  buffer_free(buffer);
}

void test_buffer_row_insert_chars_at_end(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  buffer_row_insert_chars(row, row->len, " world", 6);
  TEST_CHECK(strcmp(row->data, "Hello world") == 0);

  buffer_free(buffer);
}

void test_buffer_row_insert_chars_reallocation(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  char big_string[150];
  memset(big_string, 'x', 149);
  big_string[149] = '\0';

  buffer_row_insert_chars(row, 0, big_string, 149);
  TEST_CHECK(row->len == 149);
  TEST_CHECK(row->allocated_size >= 150);

  buffer_free(buffer);
}

void test_buffer_row_insert_chars_invalid(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Should not crash on negative index */
  buffer_row_insert_chars(row, -1, "X", 1);

  /* Should not crash on NULL row */
  buffer_row_insert_chars(NULL, 0, "X", 1);

  buffer_free(buffer);
}

TEST_LIST = {
    {"test_buffer_row_insert_char_at_start", test_buffer_row_insert_char_at_start},
    {"test_buffer_row_insert_char_at_end", test_buffer_row_insert_char_at_end},
    {"test_buffer_row_insert_char_at_middle", test_buffer_row_insert_char_at_middle},
    {"test_buffer_row_insert_char_multiple", test_buffer_row_insert_char_multiple},
    {"test_buffer_row_insert_char_reallocation",
     test_buffer_row_insert_char_reallocation},
    {"test_buffer_row_remove_char_at_start", test_buffer_row_remove_char_at_start},
    {"test_buffer_row_remove_char_at_end", test_buffer_row_remove_char_at_end},
    {"test_buffer_row_remove_char_at_middle", test_buffer_row_remove_char_at_middle},
    {"test_buffer_row_remove_char_until_empty",
     test_buffer_row_remove_char_until_empty},
    {"test_buffer_row_remove_char_invalid_index",
     test_buffer_row_remove_char_invalid_index},
    {"test_buffer_row_append_char", test_buffer_row_append_char},
    {"test_buffer_row_append_char_reallocation",
     test_buffer_row_append_char_reallocation},
    {"test_buffer_row_append_char_null_row", test_buffer_row_append_char_null_row},
    {"test_buffer_row_remove_chars_single", test_buffer_row_remove_chars_single},
    {"test_buffer_row_remove_chars_multiple", test_buffer_row_remove_chars_multiple},
    {"test_buffer_row_remove_chars_at_end", test_buffer_row_remove_chars_at_end},
    {"test_buffer_row_remove_chars_beyond_end",
     test_buffer_row_remove_chars_beyond_end},
    {"test_buffer_row_remove_chars_invalid", test_buffer_row_remove_chars_invalid},
    {"test_buffer_row_insert_chars_single", test_buffer_row_insert_chars_single},
    {"test_buffer_row_insert_chars_multiple", test_buffer_row_insert_chars_multiple},
    {"test_buffer_row_insert_chars_at_start", test_buffer_row_insert_chars_at_start},
    {"test_buffer_row_insert_chars_at_end", test_buffer_row_insert_chars_at_end},
    {"test_buffer_row_insert_chars_reallocation",
     test_buffer_row_insert_chars_reallocation},
    {"test_buffer_row_insert_chars_invalid", test_buffer_row_insert_chars_invalid},

    {NULL, NULL} /* zeroed record marking the end of the list */
};
