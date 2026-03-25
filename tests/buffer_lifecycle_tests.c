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

void test_buffer_alloc_initialization(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);
  TEST_CHECK(buffer->head == NULL);
  TEST_CHECK(buffer->tail == NULL);
  TEST_CHECK(buffer->current_row == NULL);
  TEST_CHECK(buffer->number_of_rows == 0);
  TEST_CHECK(buffer->filename == NULL);
  TEST_CHECK(buffer->filetype == NULL);
  buffer_free(buffer);
}

void test_buffer_free_null(void) {
  // Should not crash
  buffer_free(NULL);
  TEST_CHECK(1);  // If we reach here, test passed
}

void test_buffer_free_with_rows(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  TEST_CHECK(buffer->number_of_rows == 3);

  buffer_free(buffer);
  TEST_CHECK(1);  // If we reach here without memory issues, test passed
}

void test_buffer_get_first_row(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  // Empty buffer should return NULL
  TEST_CHECK(buffer_get_first_row(buffer) == NULL);
  TEST_CHECK(buffer_get_first_row(NULL) == NULL);

  buffer_append_line(buffer, "First Line");
  buffer_append_line(buffer, "Second Line");

  BufferRow* first = buffer_get_first_row(buffer);
  TEST_CHECK(first != NULL);
  TEST_CHECK(strcmp(first->data, "First Line") == 0);

  buffer_free(buffer);
}

void test_buffer_get_row_valid(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 0");
  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");

  BufferRow* row0 = buffer_get_row(buffer, 0);
  BufferRow* row1 = buffer_get_row(buffer, 1);
  BufferRow* row2 = buffer_get_row(buffer, 2);

  TEST_CHECK(row0 != NULL);
  TEST_CHECK(strcmp(row0->data, "Line 0") == 0);

  TEST_CHECK(row1 != NULL);
  TEST_CHECK(strcmp(row1->data, "Line 1") == 0);

  TEST_CHECK(row2 != NULL);
  TEST_CHECK(strcmp(row2->data, "Line 2") == 0);

  buffer_free(buffer);
}

void test_buffer_get_row_out_of_bounds(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 0");

  // Negative index should return NULL
  TEST_CHECK(buffer_get_row(buffer, -1) == NULL);

  // Index >= number_of_rows should return NULL
  TEST_CHECK(buffer_get_row(buffer, 1) == NULL);
  TEST_CHECK(buffer_get_row(buffer, 100) == NULL);

  // NULL buffer should return NULL
  TEST_CHECK(buffer_get_row(NULL, 0) == NULL);

  buffer_free(buffer);
}

void test_buffer_get_current_line(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  // Empty buffer should return NULL
  TEST_CHECK(buffer_get_current_line(buffer) == NULL);
  TEST_CHECK(buffer_get_current_line(NULL) == NULL);

  buffer_append_line(buffer, "First Line");
  TEST_CHECK(buffer_get_current_line(buffer) == buffer->current_row);
  TEST_CHECK(strcmp(buffer_get_current_line(buffer)->data, "First Line") == 0);

  buffer_append_line(buffer, "Second Line");
  // buffer_append_line only sets current_row when buffer is empty,
  // so current_row remains pointing to the first row
  TEST_CHECK(strcmp(buffer_get_current_line(buffer)->data, "First Line") == 0);

  buffer_free(buffer);
}

void test_buffer_get_number_of_lines(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  TEST_CHECK(buffer_get_number_of_lines(NULL) == 0);
  TEST_CHECK(buffer_get_number_of_lines(buffer) == 0);

  buffer_append_line(buffer, "Line 1");
  TEST_CHECK(buffer_get_number_of_lines(buffer) == 1);

  buffer_append_line(buffer, "Line 2");
  TEST_CHECK(buffer_get_number_of_lines(buffer) == 2);

  buffer_append_line(buffer, "Line 3");
  TEST_CHECK(buffer_get_number_of_lines(buffer) == 3);

  buffer_free(buffer);
}

TEST_LIST = {
    {"test_buffer_alloc_initialization", test_buffer_alloc_initialization},
    {"test_buffer_free_null", test_buffer_free_null},
    {"test_buffer_free_with_rows", test_buffer_free_with_rows},
    {"test_buffer_get_first_row", test_buffer_get_first_row},
    {"test_buffer_get_row_valid", test_buffer_get_row_valid},
    {"test_buffer_get_row_out_of_bounds", test_buffer_get_row_out_of_bounds},
    {"test_buffer_get_current_line", test_buffer_get_current_line},
    {"test_buffer_get_number_of_lines", test_buffer_get_number_of_lines},

    {NULL, NULL}  // zeroed record marking the end of the list
};
