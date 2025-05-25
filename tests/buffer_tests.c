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

void test_buffer_alloc(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);
  TEST_CHECK(buffer->head == NULL);
  TEST_CHECK(buffer->tail == NULL);
  TEST_CHECK(buffer->current_row == NULL);
  TEST_CHECK(buffer->number_of_rows == 0);
  TEST_CHECK(buffer->start_line == 0);
  TEST_CHECK(buffer->start_column == 0);
  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_next_word(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int offset = buffer_row_get_offset_to_next_word(row, 0);

  TEST_CHECK(offset == 6);  // "Hello " is 6 characters long

  offset = buffer_row_get_offset_to_next_word(row, 6);

  TEST_CHECK(offset == 5);  // no more words after "world"

  buffer_row_replace_line(row, "this      ha      fw  w w x");

  offset = buffer_row_get_offset_to_next_word(row, 0);
  TEST_CHECK(offset == 10);

  offset = buffer_row_get_offset_to_next_word(row, 10);
  TEST_CHECK(offset == 8);

  offset = buffer_row_get_offset_to_next_word(row, 18);
  TEST_CHECK(offset == 4);

  offset = buffer_row_get_offset_to_next_word(row, 22);
  TEST_CHECK(offset == 2);

  offset = buffer_row_get_offset_to_next_word(row, 24);
  TEST_CHECK(offset == 2);

  buffer_row_replace_line(row, "     this");

  offset = buffer_row_get_offset_to_next_word(row, 2);
  TEST_CHECK(offset == 3);

  offset = buffer_row_get_offset_to_next_word(row, 6);
  TEST_CHECK(offset == 3);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_prev_word(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int offset = buffer_row_get_offset_to_prev_word(row, 0);

  TEST_CHECK(offset == 0);  // "Hello " is 6 characters long

  offset = buffer_row_get_offset_to_prev_word(row, 9);
  TEST_CHECK(offset == -3);  // no more words after "world"
  offset = buffer_row_get_offset_to_prev_word(row, 6);
  TEST_CHECK(offset == -6);  // no more words after "world"

  buffer_row_replace_line(row, "  this      ha    ");

  offset = buffer_row_get_offset_to_prev_word(row, 18);
  TEST_CHECK(offset == -6);

  offset = buffer_row_get_offset_to_prev_word(row, 12);
  TEST_CHECK(offset == -10);

  offset = buffer_row_get_offset_to_prev_word(row, 2);
  TEST_CHECK(offset == 0);

  buffer_free(buffer);
}

void test_buffer_row_remove_character(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  // Remove character at index 5 ('o')
  buffer_row_remove_char(row, 4);
  TEST_CHECK(strcmp(row->data, "Hell world") == 0);
  TEST_CHECK(row->len == 10);

  // Remove character at index 0 ('H')
  buffer_row_remove_char(row, 0);
  TEST_CHECK(strcmp(row->data, "ell world") == 0);
  TEST_CHECK(row->len == 9);

  // Remove character at index 8 ('d')
  buffer_row_remove_char(row, 8);
  TEST_CHECK(strcmp(row->data, "ell worl") == 0);
  TEST_CHECK(row->len == 8);

  buffer_free(buffer);
}

void test_buffer_insert_character(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  // Insert character 'X' at index 5
  buffer_row_insert_char(row, 5, 'X');
  TEST_CHECK(strcmp(row->data, "HelloX world") == 0);
  TEST_CHECK(row->len == 12);

  // Insert character 'Y' at index 0
  buffer_row_insert_char(row, 0, 'Y');
  TEST_CHECK(strcmp(row->data, "YHelloX world") == 0);
  TEST_CHECK(row->len == 13);

  // Insert character 'Z' at index 12 (end of the line)
  buffer_row_insert_char(row, row->len, 'Z');
  TEST_CHECK(strcmp(row->data, "YHelloX worldZ") == 0);
  TEST_CHECK(row->len == 14);

  buffer_free(buffer);
}

TEST_LIST = {
  {"test_buffer_alloc", test_buffer_alloc},
  {"test_buffer_row_get_offset_to_next_word",
   test_buffer_row_get_offset_to_next_word},
  {"test_buffer_row_get_offset_to_prev_word",
   test_buffer_row_get_offset_to_prev_word},
  {"test_buffer_row_remove_character", test_buffer_row_remove_character},
  {"test_buffer_insert_character", test_buffer_insert_character},

  {NULL, NULL}  // zeroed record marking the end of the list
};