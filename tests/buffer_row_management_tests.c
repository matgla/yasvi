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

void test_buffer_append_line_single(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  bool result = buffer_append_line(buffer, "Hello World");
  TEST_CHECK(result == true);
  TEST_CHECK(buffer->number_of_rows == 1);
  TEST_CHECK(buffer->head != NULL);
  TEST_CHECK(buffer->tail != NULL);
  TEST_CHECK(buffer->current_row != NULL);
  TEST_CHECK(strcmp(buffer->current_row->data, "Hello World") == 0);

  buffer_free(buffer);
}

void test_buffer_append_line_multiple(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  TEST_CHECK(buffer->number_of_rows == 3);

  // Check that rows are properly linked
  BufferRow* row1 = buffer->head;
  BufferRow* row2 = row1->next;
  BufferRow* row3 = row2->next;

  TEST_CHECK(row1->prev == NULL);
  TEST_CHECK(row1->next == row2);
  TEST_CHECK(row2->prev == row1);
  TEST_CHECK(row2->next == row3);
  TEST_CHECK(row3->prev == row2);
  TEST_CHECK(row3->next == NULL);

  TEST_CHECK(strcmp(row1->data, "Line 1") == 0);
  TEST_CHECK(strcmp(row2->data, "Line 2") == 0);
  TEST_CHECK(strcmp(row3->data, "Line 3") == 0);

  buffer_free(buffer);
}

void test_buffer_append_line_strips_newlines(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line with newline\n");
  // current_row is set only for first append, use tail to check last appended
  TEST_CHECK(strcmp(buffer->tail->data, "Line with newline") == 0);
  TEST_CHECK(buffer->tail->len == 17);

  buffer_append_line(buffer, "Line with CRLF\r\n");
  TEST_CHECK(strcmp(buffer->tail->data, "Line with CRLF") == 0);
  TEST_CHECK(buffer->tail->len == 14);

  buffer_append_line(buffer, "Line with just CR\r");
  TEST_CHECK(strcmp(buffer->tail->data, "Line with just CR") == 0);
  TEST_CHECK(buffer->tail->len == 17);

  buffer_free(buffer);
}

void test_buffer_append_line_null(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  // NULL buffer should return false
  bool result = buffer_append_line(NULL, "test");
  TEST_CHECK(result == false);

  // NULL line should return false
  result = buffer_append_line(buffer, NULL);
  TEST_CHECK(result == false);

  buffer_free(buffer);
}

void test_buffer_remove_row_middle(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  BufferRow* middle_row = buffer->head->next;
  TEST_CHECK(strcmp(middle_row->data, "Line 2") == 0);

  buffer_remove_row(buffer, middle_row);

  TEST_CHECK(buffer->number_of_rows == 2);
  TEST_CHECK(buffer->head->next == buffer->tail);
  TEST_CHECK(buffer->tail->prev == buffer->head);
  TEST_CHECK(strcmp(buffer->head->data, "Line 1") == 0);
  TEST_CHECK(strcmp(buffer->tail->data, "Line 3") == 0);

  buffer_free(buffer);
}

void test_buffer_remove_row_head(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  BufferRow* head_row = buffer->head;
  buffer_remove_row(buffer, head_row);

  TEST_CHECK(buffer->number_of_rows == 2);
  TEST_CHECK(strcmp(buffer->head->data, "Line 2") == 0);
  TEST_CHECK(buffer->head->prev == NULL);

  buffer_free(buffer);
}

void test_buffer_remove_row_tail(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  BufferRow* tail_row = buffer->tail;
  buffer_remove_row(buffer, tail_row);

  TEST_CHECK(buffer->number_of_rows == 2);
  TEST_CHECK(strcmp(buffer->tail->data, "Line 2") == 0);
  TEST_CHECK(buffer->tail->next == NULL);

  buffer_free(buffer);
}

void test_buffer_remove_current_row_only_row(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Only Line");
  TEST_CHECK(buffer->current_row != NULL);

  int result = buffer_remove_current_row(buffer);
  TEST_CHECK(result == 0);
  TEST_CHECK(buffer->current_row == NULL);
  TEST_CHECK(buffer->number_of_rows == 0);
  TEST_CHECK(buffer->head == NULL);
  TEST_CHECK(buffer->tail == NULL);

  buffer_free(buffer);
}

void test_buffer_remove_current_row_first(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  // Move to first row
  buffer->current_row = buffer->head;

  int result = buffer_remove_current_row(buffer);
  TEST_CHECK(result == 1);  // Moved to next row
  TEST_CHECK(buffer->current_row == buffer->head);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 2") == 0);
  TEST_CHECK(buffer->number_of_rows == 2);

  buffer_free(buffer);
}

void test_buffer_remove_current_row_last(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  // Move to last row
  buffer->current_row = buffer->tail;

  int result = buffer_remove_current_row(buffer);
  TEST_CHECK(result == -1);  // Moved to previous row
  TEST_CHECK(buffer->current_row == buffer->tail);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 2") == 0);
  TEST_CHECK(buffer->number_of_rows == 2);

  buffer_free(buffer);
}

void test_buffer_remove_current_row_middle(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  // Move to middle row
  buffer->current_row = buffer->head->next;

  int result = buffer_remove_current_row(buffer);
  TEST_CHECK(result == 1);  // Moved to next row
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 3") == 0);
  TEST_CHECK(buffer->number_of_rows == 2);

  buffer_free(buffer);
}

void test_buffer_remove_row_null(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");

  // NULL buffer should not crash
  buffer_remove_row(NULL, buffer->head);

  // NULL row should not crash
  buffer_remove_row(buffer, NULL);

  // NULL buffer and row should not crash
  buffer_remove_row(NULL, NULL);

  // Original row should still be there
  TEST_CHECK(buffer->number_of_rows == 1);

  buffer_free(buffer);
}

void test_buffer_remove_current_row_null(void) {
  // NULL buffer should return 0
  int result = buffer_remove_current_row(NULL);
  TEST_CHECK(result == 0);

  Buffer* buffer = buffer_alloc();
  // No rows - should return 0
  result = buffer_remove_current_row(buffer);
  TEST_CHECK(result == 0);

  buffer_free(buffer);
}

TEST_LIST = {
    {"test_buffer_append_line_single", test_buffer_append_line_single},
    {"test_buffer_append_line_multiple", test_buffer_append_line_multiple},
    {"test_buffer_append_line_strips_newlines", test_buffer_append_line_strips_newlines},
    {"test_buffer_append_line_null", test_buffer_append_line_null},
    {"test_buffer_remove_row_middle", test_buffer_remove_row_middle},
    {"test_buffer_remove_row_head", test_buffer_remove_row_head},
    {"test_buffer_remove_row_tail", test_buffer_remove_row_tail},
    {"test_buffer_remove_current_row_only_row", test_buffer_remove_current_row_only_row},
    {"test_buffer_remove_current_row_first", test_buffer_remove_current_row_first},
    {"test_buffer_remove_current_row_last", test_buffer_remove_current_row_last},
    {"test_buffer_remove_current_row_middle", test_buffer_remove_current_row_middle},
    {"test_buffer_remove_row_null", test_buffer_remove_row_null},
    {"test_buffer_remove_current_row_null", test_buffer_remove_current_row_null},

    {NULL, NULL}  // zeroed record marking the end of the list
};
