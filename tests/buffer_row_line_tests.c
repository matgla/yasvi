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

/* Note: buffer_row_break_line is declared in header but not implemented.
   The functionality is handled by buffer_break_current_line in buffer.c */

/* Set highlight tests */

void test_buffer_row_set_highlight_normal(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Set highlight for "Hello" (indices 0-4) */
  buffer_row_set_highlight(row, 0, 5, EHighlightToken_Keyword);

  /* Check that the highlight data is set */
  for (int i = 0; i < 5; i++) {
    TEST_CHECK(row->highlight_data[i] == EHighlightToken_Keyword);
  }

  /* Check that the rest is normal */
  for (int i = 5; i < row->len; i++) {
    TEST_CHECK(row->highlight_data[i] == EHighlightToken_Normal);
  }

  buffer_free(buffer);
}

void test_buffer_row_set_highlight_partial(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Set highlight for "world" (indices 6-10) */
  buffer_row_set_highlight(row, 6, 11, EHighlightToken_String);

  /* Check */
  TEST_CHECK(row->highlight_data[0] == EHighlightToken_Normal);
  for (int i = 6; i < 11; i++) {
    TEST_CHECK(row->highlight_data[i] == EHighlightToken_String);
  }

  buffer_free(buffer);
}

void test_buffer_row_set_highlight_entire_line(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Set highlight for entire line */
  buffer_row_set_highlight(row, 0, row->len, EHighlightToken_Comment);

  /* Check all characters are highlighted */
  for (int i = 0; i < row->len; i++) {
    TEST_CHECK(row->highlight_data[i] == EHighlightToken_Comment);
  }

  buffer_free(buffer);
}

void test_buffer_row_set_highlight_invalid(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Should not crash on invalid inputs */
  buffer_row_set_highlight(NULL, 0, 5, EHighlightToken_Keyword);
  buffer_row_set_highlight(row, -1, 5, EHighlightToken_Keyword);
  buffer_row_set_highlight(row, 0, 100, EHighlightToken_Keyword);
  buffer_row_set_highlight(row, 5, 0, EHighlightToken_Keyword); /* end < start */

  buffer_free(buffer);
}

/* Highlight data allocation tests */

void test_buffer_row_highlight_data_allocated(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Highlight data should be allocated */
  TEST_CHECK(row->highlight_data != NULL);

  buffer_free(buffer);
}

void test_buffer_row_highlight_data_reallocated(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hi");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int original_allocated = row->allocated_size;

  /* Replace with longer line to trigger reallocation */
  buffer_row_replace_line(row, "Hello world, this is a much longer line");

  /* Both data and highlight_data should be reallocated */
  TEST_CHECK(row->allocated_size > original_allocated);
  TEST_CHECK(row->highlight_data != NULL);

  buffer_free(buffer);
}

TEST_LIST = {
    {"test_buffer_row_set_highlight_normal", test_buffer_row_set_highlight_normal},
    {"test_buffer_row_set_highlight_partial", test_buffer_row_set_highlight_partial},
    {"test_buffer_row_set_highlight_entire_line",
     test_buffer_row_set_highlight_entire_line},
    {"test_buffer_row_set_highlight_invalid", test_buffer_row_set_highlight_invalid},
    {"test_buffer_row_highlight_data_allocated",
     test_buffer_row_highlight_data_allocated},
    {"test_buffer_row_highlight_data_reallocated",
     test_buffer_row_highlight_data_reallocated},

    {NULL, NULL} /* zeroed record marking the end of the list */
};
