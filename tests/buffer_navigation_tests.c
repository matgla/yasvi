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

void test_buffer_scroll_rows_forward(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");
  buffer_append_line(buffer, "Line 4");
  buffer_append_line(buffer, "Line 5");

  // Start at first row
  buffer->current_row = buffer->head;
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 1") == 0);

  // Scroll forward 2 rows
  buffer_scroll_rows(buffer, 2);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 3") == 0);

  // Scroll forward 1 more row
  buffer_scroll_rows(buffer, 1);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 4") == 0);

  buffer_free(buffer);
}

void test_buffer_scroll_rows_backward(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");
  buffer_append_line(buffer, "Line 4");
  buffer_append_line(buffer, "Line 5");

  // Start at last row
  buffer->current_row = buffer->tail;
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 5") == 0);

  // Scroll backward 2 rows
  buffer_scroll_rows(buffer, -2);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 3") == 0);

  // Scroll backward 1 more row
  buffer_scroll_rows(buffer, -1);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 2") == 0);

  buffer_free(buffer);
}

void test_buffer_scroll_rows_past_end(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  // Start at first row
  buffer->current_row = buffer->head;

  // Try to scroll past end
  buffer_scroll_rows(buffer, 10);
  // Should stop at last row
  TEST_CHECK(buffer->current_row == buffer->tail);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 3") == 0);

  buffer_free(buffer);
}

void test_buffer_scroll_rows_past_start(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  // Start at last row
  buffer->current_row = buffer->tail;

  // Try to scroll past start
  buffer_scroll_rows(buffer, -10);
  // Should stop at first row
  TEST_CHECK(buffer->current_row == buffer->head);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 1") == 0);

  buffer_free(buffer);
}

void test_buffer_scroll_rows_zero(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");

  buffer->current_row = buffer->head;

  // Scroll by 0 should not move
  buffer_scroll_rows(buffer, 0);
  TEST_CHECK(buffer->current_row == buffer->head);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 1") == 0);

  buffer_free(buffer);
}

void test_buffer_scroll_rows_null(void) {
  // NULL buffer should not crash
  buffer_scroll_rows(NULL, 5);
  TEST_CHECK(1);  // If we reach here, test passed
}

void test_buffer_scroll_to_top(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  // Start at last row
  buffer->current_row = buffer->tail;
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 3") == 0);

  buffer_scroll_to_top(buffer);
  TEST_CHECK(buffer->current_row == buffer->head);
  TEST_CHECK(strcmp(buffer->current_row->data, "Line 1") == 0);

  buffer_free(buffer);
}

void test_buffer_scroll_to_top_null(void) {
  // NULL buffer should not crash
  buffer_scroll_to_top(NULL);
  TEST_CHECK(1);  // If we reach here, test passed
}

void test_buffer_current_is_first_row(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  // NULL buffer should return false
  TEST_CHECK(buffer_current_is_first_row(NULL) == false);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  buffer->current_row = buffer->head;
  TEST_CHECK(buffer_current_is_first_row(buffer) == true);

  buffer->current_row = buffer->head->next;
  TEST_CHECK(buffer_current_is_first_row(buffer) == false);

  buffer->current_row = buffer->tail;
  TEST_CHECK(buffer_current_is_first_row(buffer) == false);

  buffer_free(buffer);
}

void test_buffer_current_is_first_row_null_current(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer->current_row = NULL;

  TEST_CHECK(buffer_current_is_first_row(buffer) == false);

  buffer_free(buffer);
}

void test_buffer_current_is_last_row(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  // NULL buffer should return false
  TEST_CHECK(buffer_current_is_last_row(NULL) == false);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  buffer->current_row = buffer->head;
  TEST_CHECK(buffer_current_is_last_row(buffer) == false);

  buffer->current_row = buffer->head->next;
  TEST_CHECK(buffer_current_is_last_row(buffer) == false);

  buffer->current_row = buffer->tail;
  TEST_CHECK(buffer_current_is_last_row(buffer) == true);

  buffer_free(buffer);
}

void test_buffer_current_is_last_row_null_current(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer->current_row = NULL;

  TEST_CHECK(buffer_current_is_last_row(buffer) == false);

  buffer_free(buffer);
}

TEST_LIST = {
    {"test_buffer_scroll_rows_forward", test_buffer_scroll_rows_forward},
    {"test_buffer_scroll_rows_backward", test_buffer_scroll_rows_backward},
    {"test_buffer_scroll_rows_past_end", test_buffer_scroll_rows_past_end},
    {"test_buffer_scroll_rows_past_start", test_buffer_scroll_rows_past_start},
    {"test_buffer_scroll_rows_zero", test_buffer_scroll_rows_zero},
    {"test_buffer_scroll_rows_null", test_buffer_scroll_rows_null},
    {"test_buffer_scroll_to_top", test_buffer_scroll_to_top},
    {"test_buffer_scroll_to_top_null", test_buffer_scroll_to_top_null},
    {"test_buffer_current_is_first_row", test_buffer_current_is_first_row},
    {"test_buffer_current_is_first_row_null_current", test_buffer_current_is_first_row_null_current},
    {"test_buffer_current_is_last_row", test_buffer_current_is_last_row},
    {"test_buffer_current_is_last_row_null_current", test_buffer_current_is_last_row_null_current},

    {NULL, NULL}  // zeroed record marking the end of the list
};
