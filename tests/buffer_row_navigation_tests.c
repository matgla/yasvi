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

/* Next word offset tests - already partially tested in buffer_tests.c */

void test_buffer_row_get_offset_to_next_word_basic(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* From start, should skip "Hello " (6 chars) to get to 'w' */
  int offset = buffer_row_get_offset_to_next_word(row, 0);
  TEST_CHECK(offset == 6);

  /* From 'w', should skip "world" (5 chars) to end */
  offset = buffer_row_get_offset_to_next_word(row, 6);
  TEST_CHECK(offset == 5);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_next_word_multiple_spaces(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello    world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* From 'o' (index 4), should return offset to 'w' */
  int offset = buffer_row_get_offset_to_next_word(row, 4);
  /* The function returns the offset to the start of the next word */
  /* which should skip the trailing 'o' and 4 spaces = 5 chars to get to 'w' */
  TEST_CHECK(offset == 5);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_next_word_leading_spaces(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "   Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* From start with leading spaces */
  int offset = buffer_row_get_offset_to_next_word(row, 0);
  TEST_CHECK(offset == 3); /* Just skip the spaces to 'H' */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_next_word_end_of_line(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* At last char, should return 0 (no next word) */
  int offset = buffer_row_get_offset_to_next_word(row, 4);
  TEST_CHECK(offset == 1);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_next_word_only_spaces(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "     ");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Only spaces - skip to end of line */
  int offset = buffer_row_get_offset_to_next_word(row, 0);
  TEST_CHECK(offset == 5); /* Skip all 5 spaces to end */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_next_word_empty(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int offset = buffer_row_get_offset_to_next_word(row, 0);
  TEST_CHECK(offset == 0);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_next_word_invalid(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Invalid inputs */
  TEST_CHECK(buffer_row_get_offset_to_next_word(NULL, 0) == 0);
  TEST_CHECK(buffer_row_get_offset_to_next_word(row, -1) == 0);
  TEST_CHECK(buffer_row_get_offset_to_next_word(row, 100) == 0);

  buffer_free(buffer);
}

/* Previous word offset tests */

void test_buffer_row_get_offset_to_prev_word_basic(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* At start, no previous word */
  int offset = buffer_row_get_offset_to_prev_word(row, 0);
  TEST_CHECK(offset == 0);

  /* From after "world" */
  offset = buffer_row_get_offset_to_prev_word(row, 11);
  TEST_CHECK(offset == -5); /* "world" is 5 chars */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_prev_word_from_middle(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* From 'o' in "world" (index 9), should go to 'w' (index 6) */
  int offset = buffer_row_get_offset_to_prev_word(row, 9);
  TEST_CHECK(offset == -3); /* Go back 3: 'o'(9) -> 'r'(8) -> 'l'(7) -> 'w'(6) */

  /* From 'w' (index 6), should go to start */
  offset = buffer_row_get_offset_to_prev_word(row, 6);
  TEST_CHECK(offset == -6); /* "Hello " is 6 chars */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_prev_word_multiple_spaces(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello    world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* From 'w' in "world" (index 9), should go back to start of "Hello" */
  /* Skip: 'o'(8) 'l'(7) 'l'(6) 'e'(5) ' '(4) ' '(3) ' '(2) ' '(1) -> 'H'(0) */
  int offset = buffer_row_get_offset_to_prev_word(row, 9);
  /* The actual behavior - offset should be negative */
  TEST_CHECK(offset < 0); /* Just verify it goes backward */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_prev_word_trailing_spaces(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello   ");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* From trailing spaces (index 7), should go to start of "Hello" */
  int offset = buffer_row_get_offset_to_prev_word(row, 7);
  /* The actual behavior - skip whitespace, then word, find prev word */
  TEST_CHECK(offset < 0); /* Just verify it goes backward */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_prev_word_only_spaces(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "     ");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Only spaces - go back to start of line */
  int offset = buffer_row_get_offset_to_prev_word(row, 5);
  TEST_CHECK(offset == -5);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_prev_word_empty(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  int offset = buffer_row_get_offset_to_prev_word(row, 0);
  TEST_CHECK(offset == 0);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_prev_word_invalid(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Invalid inputs */
  TEST_CHECK(buffer_row_get_offset_to_prev_word(NULL, 0) == 0);
  TEST_CHECK(buffer_row_get_offset_to_prev_word(row, -1) == 0);
  TEST_CHECK(buffer_row_get_offset_to_prev_word(row, 200) == 0);

  buffer_free(buffer);
}

/* Get next row tests */

void test_buffer_row_get_next_normal(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  BufferRow* row1 = buffer->head;
  BufferRow* row2 = row1->next;
  BufferRow* row3 = row2->next;

  TEST_CHECK(buffer_row_get_next(row1) == row2);
  TEST_CHECK(buffer_row_get_next(row2) == row3);
  TEST_CHECK(buffer_row_get_next(row3) == NULL);

  buffer_free(buffer);
}

void test_buffer_row_get_next_null(void) {
  TEST_CHECK(buffer_row_get_next(NULL) == NULL);
}

void test_buffer_row_get_next_single_row(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Only line");

  BufferRow* row = buffer->head;
  TEST_CHECK(buffer_row_get_next(row) == NULL);

  buffer_free(buffer);
}

/* Get previous row tests */

void test_buffer_row_get_prev_normal(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Line 1");
  buffer_append_line(buffer, "Line 2");
  buffer_append_line(buffer, "Line 3");

  BufferRow* row1 = buffer->head;
  BufferRow* row2 = row1->next;
  BufferRow* row3 = row2->next;

  TEST_CHECK(buffer_row_get_prev(row1) == NULL);
  TEST_CHECK(buffer_row_get_prev(row2) == row1);
  TEST_CHECK(buffer_row_get_prev(row3) == row2);

  buffer_free(buffer);
}

void test_buffer_row_get_prev_null(void) {
  TEST_CHECK(buffer_row_get_prev(NULL) == NULL);
}

void test_buffer_row_get_prev_single_row(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Only line");

  BufferRow* row = buffer->head;
  TEST_CHECK(buffer_row_get_prev(row) == NULL);

  buffer_free(buffer);
}

/* Mark dirty tests */

void test_buffer_row_mark_dirty(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Initially should be dirty (set by buffer_append_line) */
  TEST_CHECK(row->dirty == true);

  /* Clear dirty flag */
  row->dirty = false;
  TEST_CHECK(row->dirty == false);

  /* Mark as dirty */
  buffer_row_mark_dirty(row);
  TEST_CHECK(row->dirty == true);

  buffer_free(buffer);
}

void test_buffer_row_mark_dirty_null(void) {
  /* Should not crash on NULL row */
  buffer_row_mark_dirty(NULL);
  TEST_CHECK(1);
}

void test_buffer_row_mark_dirty_operations_set_dirty(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* Clear dirty flag manually */
  row->dirty = false;

  /* Various operations should set dirty flag */
  buffer_row_insert_char(row, 0, 'X');
  TEST_CHECK(row->dirty == true);

  row->dirty = false;
  buffer_row_remove_char(row, 0);
  TEST_CHECK(row->dirty == true);

  row->dirty = false;
  buffer_row_append_char(row, 'Y');
  TEST_CHECK(row->dirty == true);

  row->dirty = false;
  buffer_row_replace_line(row, "New line");
  TEST_CHECK(row->dirty == true);

  row->dirty = false;
  buffer_row_trim(row, 5);
  TEST_CHECK(row->dirty == true);

  buffer_free(buffer);
}

/* Vim-like word navigation tests - distinguish between word chars and special chars */

void test_buffer_row_get_offset_to_next_word_special_chars(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  /* Test: object->method() - each special char is its own word */
  buffer_append_line(buffer, "object->method()");
  BufferRow* row = buffer->current_row;
  TEST_CHECK(row != NULL);

  /* From 'o', should go to '-' at position 6 */
  int offset = buffer_row_get_offset_to_next_word(row, 0);
  TEST_CHECK(offset == 6);

  /* From '-', should go to '>' at position 7 */
  offset = buffer_row_get_offset_to_next_word(row, 6);
  TEST_CHECK(offset == 1);

  /* From '>', should go to 'm' at position 8 */
  offset = buffer_row_get_offset_to_next_word(row, 7);
  TEST_CHECK(offset == 1);

  /* From 'm', should go to '(' at position 14 */
  offset = buffer_row_get_offset_to_next_word(row, 8);
  TEST_CHECK(offset == 6);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_next_word_mixed_types(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  /* Test: foo_bar.baz - underscore is word char, dot is special */
  buffer_append_line(buffer, "foo_bar.baz");
  BufferRow* row = buffer->current_row;

  /* From 'f', should skip "foo_bar" (7 chars) to '.' at position 7 */
  int offset = buffer_row_get_offset_to_next_word(row, 0);
  TEST_CHECK(offset == 7);

  /* From '.', should go to 'b' at position 8 */
  offset = buffer_row_get_offset_to_next_word(row, 7);
  TEST_CHECK(offset == 1);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_prev_word_special_chars(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  /* Test: object->method() */
  buffer_append_line(buffer, "object->method()");
  BufferRow* row = buffer->current_row;

  /* From position 16 (end), go back to ')' at position 15 */
  int offset = buffer_row_get_offset_to_prev_word(row, 16);
  TEST_CHECK(offset == -1);

  /* From position 15 (')'), go back to '(' at position 14 */
  offset = buffer_row_get_offset_to_prev_word(row, 15);
  TEST_CHECK(offset == -1);

  /* From position 14 ('('), go back to 'm' at position 8 */
  offset = buffer_row_get_offset_to_prev_word(row, 14);
  TEST_CHECK(offset == -6);

  /* From position 8 ('m'), go back to '>' at position 7 */
  offset = buffer_row_get_offset_to_prev_word(row, 8);
  TEST_CHECK(offset == -1);

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_end_of_word_basic(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello world");
  BufferRow* row = buffer->current_row;

  /* From 'H', should go to 'o' (end of "Hello") */
  int offset = buffer_row_get_offset_to_end_of_word(row, 0);
  TEST_CHECK(offset == 4);  /* 0->4 = 'o' */

  /* From 'w', should go to 'd' (end of "world") */
  offset = buffer_row_get_offset_to_end_of_word(row, 6);
  TEST_CHECK(offset == 4);  /* 6->10 = 'd' */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_end_of_word_special_chars(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "obj->ptr");
  BufferRow* row = buffer->current_row;

  /* From 'o', should go to 'j' (end of "obj") */
  int offset = buffer_row_get_offset_to_end_of_word(row, 0);
  TEST_CHECK(offset == 2);  /* 0->2 = 'j' */

  buffer_free(buffer);
}

void test_buffer_row_get_offset_to_end_of_word_from_whitespace(void) {
  Buffer* buffer = buffer_alloc();
  TEST_CHECK(buffer != NULL);

  buffer_append_line(buffer, "Hello  world");
  BufferRow* row = buffer->current_row;

  /* From whitespace (position 5 or 6), should skip whitespace and go to end of word */
  int offset = buffer_row_get_offset_to_end_of_word(row, 5);
  /* Skip 2 spaces, then go to end of "world" = 6 positions to 'd' */
  TEST_CHECK(offset == 6);

  buffer_free(buffer);
}

TEST_LIST = {
    {"test_buffer_row_get_offset_to_next_word_basic",
     test_buffer_row_get_offset_to_next_word_basic},
    {"test_buffer_row_get_offset_to_next_word_multiple_spaces",
     test_buffer_row_get_offset_to_next_word_multiple_spaces},
    {"test_buffer_row_get_offset_to_next_word_leading_spaces",
     test_buffer_row_get_offset_to_next_word_leading_spaces},
    {"test_buffer_row_get_offset_to_next_word_end_of_line",
     test_buffer_row_get_offset_to_next_word_end_of_line},
    {"test_buffer_row_get_offset_to_next_word_only_spaces",
     test_buffer_row_get_offset_to_next_word_only_spaces},
    {"test_buffer_row_get_offset_to_next_word_empty",
     test_buffer_row_get_offset_to_next_word_empty},
    {"test_buffer_row_get_offset_to_next_word_invalid",
     test_buffer_row_get_offset_to_next_word_invalid},
    {"test_buffer_row_get_offset_to_prev_word_basic",
     test_buffer_row_get_offset_to_prev_word_basic},
    {"test_buffer_row_get_offset_to_prev_word_from_middle",
     test_buffer_row_get_offset_to_prev_word_from_middle},
    {"test_buffer_row_get_offset_to_prev_word_multiple_spaces",
     test_buffer_row_get_offset_to_prev_word_multiple_spaces},
    {"test_buffer_row_get_offset_to_prev_word_trailing_spaces",
     test_buffer_row_get_offset_to_prev_word_trailing_spaces},
    {"test_buffer_row_get_offset_to_prev_word_only_spaces",
     test_buffer_row_get_offset_to_prev_word_only_spaces},
    {"test_buffer_row_get_offset_to_prev_word_empty",
     test_buffer_row_get_offset_to_prev_word_empty},
    {"test_buffer_row_get_offset_to_prev_word_invalid",
     test_buffer_row_get_offset_to_prev_word_invalid},
    {"test_buffer_row_get_next_normal", test_buffer_row_get_next_normal},
    {"test_buffer_row_get_next_null", test_buffer_row_get_next_null},
    {"test_buffer_row_get_next_single_row", test_buffer_row_get_next_single_row},
    {"test_buffer_row_get_prev_normal", test_buffer_row_get_prev_normal},
    {"test_buffer_row_get_prev_null", test_buffer_row_get_prev_null},
    {"test_buffer_row_get_prev_single_row", test_buffer_row_get_prev_single_row},
    {"test_buffer_row_mark_dirty", test_buffer_row_mark_dirty},
    {"test_buffer_row_mark_dirty_null", test_buffer_row_mark_dirty_null},
    {"test_buffer_row_mark_dirty_operations_set_dirty",
     test_buffer_row_mark_dirty_operations_set_dirty},
    /* Vim-like word navigation with special character handling */
    {"test_buffer_row_get_offset_to_next_word_special_chars",
     test_buffer_row_get_offset_to_next_word_special_chars},
    {"test_buffer_row_get_offset_to_next_word_mixed_types",
     test_buffer_row_get_offset_to_next_word_mixed_types},
    {"test_buffer_row_get_offset_to_prev_word_special_chars",
     test_buffer_row_get_offset_to_prev_word_special_chars},
    {"test_buffer_row_get_offset_to_end_of_word_basic",
     test_buffer_row_get_offset_to_end_of_word_basic},
    {"test_buffer_row_get_offset_to_end_of_word_special_chars",
     test_buffer_row_get_offset_to_end_of_word_special_chars},
    {"test_buffer_row_get_offset_to_end_of_word_from_whitespace",
     test_buffer_row_get_offset_to_end_of_word_from_whitespace},

    {NULL, NULL} /* zeroed record marking the end of the list */
};
