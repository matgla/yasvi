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

#include "../search.h"
#include "../buffer.h"
#include "../buffer_row.h"

/* ============================================================================
 * Search State Tests
 * ============================================================================ */

void test_search_init(void) {
  SearchState state;
  search_init(&state);

  TEST_CHECK(state.pattern == NULL);
  TEST_CHECK(state.last_match_line == 0);
  TEST_CHECK(state.last_match_col == 0);
  TEST_CHECK(state.forward == true);
  TEST_CHECK(state.case_sensitive == true);
}

void test_search_init_null(void) {
  // Should not crash
  search_init(NULL);
}

void test_search_set_pattern(void) {
  SearchState state;
  search_init(&state);

  search_set_pattern(&state, "hello", true);

  TEST_CHECK(state.pattern != NULL);
  TEST_CHECK(strcmp(state.pattern, "hello") == 0);
  TEST_CHECK(state.forward == true);

  search_deinit(&state);
}

void test_search_set_pattern_backward(void) {
  SearchState state;
  search_init(&state);

  search_set_pattern(&state, "world", false);

  TEST_CHECK(state.pattern != NULL);
  TEST_CHECK(strcmp(state.pattern, "world") == 0);
  TEST_CHECK(state.forward == false);

  search_deinit(&state);
}

void test_search_set_pattern_overwrite(void) {
  SearchState state;
  search_init(&state);

  search_set_pattern(&state, "first", true);
  search_set_pattern(&state, "second", false);

  TEST_CHECK(strcmp(state.pattern, "second") == 0);
  TEST_CHECK(state.forward == false);

  search_deinit(&state);
}

void test_search_clear_pattern(void) {
  SearchState state;
  search_init(&state);

  search_set_pattern(&state, "test", true);
  TEST_CHECK(state.pattern != NULL);

  search_clear_pattern(&state);
  TEST_CHECK(state.pattern == NULL);

  search_deinit(&state);
}

void test_search_deinit(void) {
  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "cleanup", true);

  search_deinit(&state);

  TEST_CHECK(state.pattern == NULL);
}

void test_search_set_position(void) {
  SearchState state;
  search_init(&state);

  search_set_position(&state, 5, 10);

  TEST_CHECK(state.last_match_line == 5);
  TEST_CHECK(state.last_match_col == 10);

  search_deinit(&state);
}

/* ============================================================================
 * String Search Tests
 * ============================================================================ */

void test_search_find_in_string_basic(void) {
  const char* haystack = "hello world";
  int pos = search_find_in_string(haystack, "world", 0, true);
  TEST_CHECK(pos == 6);
}

void test_search_find_in_string_not_found(void) {
  const char* haystack = "hello world";
  int pos = search_find_in_string(haystack, "foo", 0, true);
  TEST_CHECK(pos == -1);
}

void test_search_find_in_string_start_offset(void) {
  const char* haystack = "hello hello";
  int pos = search_find_in_string(haystack, "hello", 1, true);
  TEST_CHECK(pos == 6);
}

void test_search_find_in_string_multiple_occurrences(void) {
  const char* haystack = "abc abc abc";
  int pos1 = search_find_in_string(haystack, "abc", 0, true);
  int pos2 = search_find_in_string(haystack, "abc", 1, true);
  int pos3 = search_find_in_string(haystack, "abc", 5, true);

  TEST_CHECK(pos1 == 0);
  TEST_CHECK(pos2 == 4);
  TEST_CHECK(pos3 == 8);
}

void test_search_find_in_string_empty_needle(void) {
  const char* haystack = "hello";
  int pos = search_find_in_string(haystack, "", 0, true);
  TEST_CHECK(pos == -1);
}

void test_search_find_in_string_empty_haystack(void) {
  const char* haystack = "";
  int pos = search_find_in_string(haystack, "test", 0, true);
  TEST_CHECK(pos == -1);
}

void test_search_find_in_string_null(void) {
  int pos = search_find_in_string(NULL, "test", 0, true);
  TEST_CHECK(pos == -1);
}

void test_search_find_in_string_case_sensitive(void) {
  const char* haystack = "Hello World";
  int pos1 = search_find_in_string(haystack, "world", 0, true);
  int pos2 = search_find_in_string(haystack, "World", 0, true);

  TEST_CHECK(pos1 == -1);  // Case sensitive, should not match
  TEST_CHECK(pos2 == 6);
}

void test_search_find_in_string_case_insensitive(void) {
  const char* haystack = "Hello World";
  int pos1 = search_find_in_string(haystack, "WORLD", 0, false);
  int pos2 = search_find_in_string(haystack, "world", 0, false);
  int pos3 = search_find_in_string(haystack, "Hello", 0, false);

  TEST_CHECK(pos1 == 6);
  TEST_CHECK(pos2 == 6);
  TEST_CHECK(pos3 == 0);
}

/* ============================================================================
 * Buffer Search Tests
 * ============================================================================ */

void test_search_find_next_basic(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello world");
  buffer_append_line(buffer, "foo bar");
  buffer_append_line(buffer, "hello again");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "hello", true);
  search_set_position(&state, 0, 0);

  bool found = search_find_next(&state, buffer);

  TEST_CHECK(found == true);
  TEST_CHECK(state.last_match_line == 0);
  TEST_CHECK(state.last_match_col == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_next_multiple_lines(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "first line");
  buffer_append_line(buffer, "second line");
  buffer_append_line(buffer, "third line");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "line", true);
  search_set_position(&state, 0, 0);

  bool found1 = search_find_next(&state, buffer);
  TEST_CHECK(found1 == true);
  TEST_CHECK(state.last_match_line == 0);
  TEST_CHECK(state.last_match_col == 6);

  bool found2 = search_find_next(&state, buffer);
  TEST_CHECK(found2 == true);
  TEST_CHECK(state.last_match_line == 1);
  TEST_CHECK(state.last_match_col == 7);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_next_not_found(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello world");
  buffer_append_line(buffer, "foo bar");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "xyz", true);
  search_set_position(&state, 0, 0);

  bool found = search_find_next(&state, buffer);

  TEST_CHECK(found == false);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_next_wrap_around(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "first");
  buffer_append_line(buffer, "second");
  buffer_append_line(buffer, "first");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "first", true);
  search_set_position(&state, 2, 0);  // Start from last line

  // First search should find on line 2
  bool found1 = search_find_next(&state, buffer);
  TEST_CHECK(found1 == true);
  TEST_CHECK(state.last_match_line == 2);

  // Second search should wrap to line 0
  bool found2 = search_find_next(&state, buffer);
  TEST_CHECK(found2 == true);
  TEST_CHECK(state.last_match_line == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_next_same_line_multiple(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "abc abc abc");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "abc", true);
  search_set_position(&state, 0, 0);

  bool found1 = search_find_next(&state, buffer);
  TEST_CHECK(found1 == true);
  TEST_CHECK(state.last_match_col == 0);

  bool found2 = search_find_next(&state, buffer);
  TEST_CHECK(found2 == true);
  TEST_CHECK(state.last_match_col == 4);

  bool found3 = search_find_next(&state, buffer);
  TEST_CHECK(found3 == true);
  TEST_CHECK(state.last_match_col == 8);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_next_empty_buffer(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "\n");  // Empty buffer has one empty line

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "test", true);
  search_set_position(&state, 0, 0);

  bool found = search_find_next(&state, buffer);

  TEST_CHECK(found == false);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_next_null_buffer(void) {
  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "test", true);

  bool found = search_find_next(&state, NULL);

  TEST_CHECK(found == false);

  search_deinit(&state);
}

void test_search_find_next_no_pattern(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello");

  SearchState state;
  search_init(&state);
  // Don't set pattern

  bool found = search_find_next(&state, buffer);

  TEST_CHECK(found == false);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_prev_basic(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "first");
  buffer_append_line(buffer, "second");
  buffer_append_line(buffer, "third");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "second", false);
  search_set_position(&state, 2, 0);  // Start from last line

  bool found = search_find_prev(&state, buffer);

  TEST_CHECK(found == true);
  TEST_CHECK(state.last_match_line == 1);
  TEST_CHECK(state.last_match_col == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_prev_multiple(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "abc");
  buffer_append_line(buffer, "def");
  buffer_append_line(buffer, "abc");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "abc", false);
  search_set_position(&state, 2, 0);

  bool found1 = search_find_prev(&state, buffer);
  TEST_CHECK(found1 == true);
  TEST_CHECK(state.last_match_line == 2);

  bool found2 = search_find_prev(&state, buffer);
  TEST_CHECK(found2 == true);
  TEST_CHECK(state.last_match_line == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_prev_wrap_around(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "target");
  buffer_append_line(buffer, "middle");
  buffer_append_line(buffer, "target");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "target", false);
  search_set_position(&state, 0, 0);

  // First search backwards from line 0 finds the match at line 0 (vim behavior)
  bool found = search_find_prev(&state, buffer);
  TEST_CHECK(found == true);
  TEST_CHECK(state.last_match_line == 0);

  // Subsequent search should wrap to line 2
  found = search_find_prev(&state, buffer);
  TEST_CHECK(found == true);
  TEST_CHECK(state.last_match_line == 2);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_find_prev_same_line(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "abc abc abc");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "abc", false);
  search_set_position(&state, 0, 10);  // Near end of line

  bool found1 = search_find_prev(&state, buffer);
  TEST_CHECK(found1 == true);
  TEST_CHECK(state.last_match_col == 8);

  bool found2 = search_find_prev(&state, buffer);
  TEST_CHECK(found2 == true);
  TEST_CHECK(state.last_match_col == 4);

  bool found3 = search_find_prev(&state, buffer);
  TEST_CHECK(found3 == true);
  TEST_CHECK(state.last_match_col == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

/* ============================================================================
 * Replace Tests
 * ============================================================================ */

void test_search_replace_line_basic(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello world");

  ReplaceResult result = search_replace_line(buffer, 0, "world", "universe", false);

  TEST_CHECK(result.replacements == 1);
  TEST_CHECK(result.lines_affected == 1);

  BufferRow* row = buffer_get_row(buffer, 0);
  TEST_CHECK(strcmp(row->data, "hello universe") == 0);

  buffer_free(buffer);
}

void test_search_replace_line_global(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "abc abc abc");

  ReplaceResult result = search_replace_line(buffer, 0, "abc", "xyz", true);

  TEST_CHECK(result.replacements == 3);
  TEST_CHECK(result.lines_affected == 1);

  BufferRow* row = buffer_get_row(buffer, 0);
  TEST_CHECK(strcmp(row->data, "xyz xyz xyz") == 0);

  buffer_free(buffer);
}

void test_search_replace_line_not_found(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello world");

  ReplaceResult result = search_replace_line(buffer, 0, "xyz", "abc", false);

  TEST_CHECK(result.replacements == 0);
  TEST_CHECK(result.lines_affected == 0);

  BufferRow* row = buffer_get_row(buffer, 0);
  TEST_CHECK(strcmp(row->data, "hello world") == 0);

  buffer_free(buffer);
}

void test_search_replace_line_first_only(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "abc abc abc");

  ReplaceResult result = search_replace_line(buffer, 0, "abc", "xyz", false);

  TEST_CHECK(result.replacements == 1);

  BufferRow* row = buffer_get_row(buffer, 0);
  TEST_CHECK(strcmp(row->data, "xyz abc abc") == 0);

  buffer_free(buffer);
}

void test_search_replace_line_empty_replacement(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello world");

  ReplaceResult result = search_replace_line(buffer, 0, "world", "", false);

  TEST_CHECK(result.replacements == 1);

  BufferRow* row = buffer_get_row(buffer, 0);
  TEST_CHECK(strcmp(row->data, "hello ") == 0);

  buffer_free(buffer);
}

void test_search_replace_line_longer_replacement(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hi");

  ReplaceResult result = search_replace_line(buffer, 0, "hi", "hello world", false);

  TEST_CHECK(result.replacements == 1);

  BufferRow* row = buffer_get_row(buffer, 0);
  TEST_CHECK(strcmp(row->data, "hello world") == 0);

  buffer_free(buffer);
}

void test_search_replace_line_invalid_line(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello");

  ReplaceResult result = search_replace_line(buffer, 5, "hello", "world", false);

  TEST_CHECK(result.replacements == 0);
  TEST_CHECK(result.lines_affected == 0);

  buffer_free(buffer);
}

void test_search_replace_line_null_args(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello");

  ReplaceResult result = search_replace_line(NULL, 0, "hello", "world", false);
  TEST_CHECK(result.replacements == 0);

  result = search_replace_line(buffer, 0, NULL, "world", false);
  TEST_CHECK(result.replacements == 0);

  result = search_replace_line(buffer, 0, "hello", NULL, false);
  TEST_CHECK(result.replacements == 0);

  buffer_free(buffer);
}

void test_search_replace_all_basic(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello world");
  buffer_append_line(buffer, "hello again");
  buffer_append_line(buffer, "goodbye world");

  ReplaceResult result = search_replace_all(buffer, "world", "universe", false);

  TEST_CHECK(result.replacements == 2);
  TEST_CHECK(result.lines_affected == 2);

  BufferRow* row0 = buffer_get_row(buffer, 0);
  BufferRow* row1 = buffer_get_row(buffer, 1);
  BufferRow* row2 = buffer_get_row(buffer, 2);

  TEST_CHECK(strcmp(row0->data, "hello universe") == 0);
  TEST_CHECK(strcmp(row1->data, "hello again") == 0);
  TEST_CHECK(strcmp(row2->data, "goodbye universe") == 0);

  buffer_free(buffer);
}

void test_search_replace_all_global(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "abc def abc");
  buffer_append_line(buffer, "abc xyz");

  ReplaceResult result = search_replace_all(buffer, "abc", "XYZ", true);

  TEST_CHECK(result.replacements == 3);

  BufferRow* row0 = buffer_get_row(buffer, 0);
  BufferRow* row1 = buffer_get_row(buffer, 1);

  TEST_CHECK(strcmp(row0->data, "XYZ def XYZ") == 0);
  TEST_CHECK(strcmp(row1->data, "XYZ xyz") == 0);

  buffer_free(buffer);
}

void test_search_replace_all_not_found(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello");
  buffer_append_line(buffer, "world");

  ReplaceResult result = search_replace_all(buffer, "xyz", "abc", false);

  TEST_CHECK(result.replacements == 0);
  TEST_CHECK(result.lines_affected == 0);

  buffer_free(buffer);
}

void test_search_replace_range_basic(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "line 0");
  buffer_append_line(buffer, "target line");
  buffer_append_line(buffer, "target line");
  buffer_append_line(buffer, "line 3");

  ReplaceResult result = search_replace_range(buffer, 1, 2, "target", "modified", false);

  TEST_CHECK(result.replacements == 2);
  TEST_CHECK(result.lines_affected == 2);

  BufferRow* row0 = buffer_get_row(buffer, 0);
  BufferRow* row1 = buffer_get_row(buffer, 1);
  BufferRow* row2 = buffer_get_row(buffer, 2);
  BufferRow* row3 = buffer_get_row(buffer, 3);

  TEST_CHECK(strcmp(row0->data, "line 0") == 0);
  TEST_CHECK(strcmp(row1->data, "modified line") == 0);
  TEST_CHECK(strcmp(row2->data, "modified line") == 0);
  TEST_CHECK(strcmp(row3->data, "line 3") == 0);

  buffer_free(buffer);
}

void test_search_replace_range_out_of_bounds(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "line 0");
  buffer_append_line(buffer, "line 1");

  // Range beyond buffer size
  ReplaceResult result = search_replace_range(buffer, 5, 10, "line", "row", false);

  TEST_CHECK(result.replacements == 0);

  buffer_free(buffer);
}

void test_search_replace_range_negative_start(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello");
  buffer_append_line(buffer, "hello");

  ReplaceResult result = search_replace_range(buffer, -5, 1, "hello", "hi", false);

  TEST_CHECK(result.replacements == 2);

  buffer_free(buffer);
}

void test_search_replace_range_inverted(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "hello");
  buffer_append_line(buffer, "hello");

  // start_line > end_line
  ReplaceResult result = search_replace_range(buffer, 1, 0, "hello", "hi", false);

  TEST_CHECK(result.replacements == 0);

  buffer_free(buffer);
}

/* ============================================================================
 * Test List
 * ============================================================================ */

TEST_LIST = {
    /* Search State Tests */
    {"search_init", test_search_init},
    {"search_init_null", test_search_init_null},
    {"search_set_pattern", test_search_set_pattern},
    {"search_set_pattern_backward", test_search_set_pattern_backward},
    {"search_set_pattern_overwrite", test_search_set_pattern_overwrite},
    {"search_clear_pattern", test_search_clear_pattern},
    {"search_deinit", test_search_deinit},
    {"search_set_position", test_search_set_position},

    /* String Search Tests */
    {"search_find_in_string_basic", test_search_find_in_string_basic},
    {"search_find_in_string_not_found", test_search_find_in_string_not_found},
    {"search_find_in_string_start_offset", test_search_find_in_string_start_offset},
    {"search_find_in_string_multiple_occurrences", test_search_find_in_string_multiple_occurrences},
    {"search_find_in_string_empty_needle", test_search_find_in_string_empty_needle},
    {"search_find_in_string_empty_haystack", test_search_find_in_string_empty_haystack},
    {"search_find_in_string_null", test_search_find_in_string_null},
    {"search_find_in_string_case_sensitive", test_search_find_in_string_case_sensitive},
    {"search_find_in_string_case_insensitive", test_search_find_in_string_case_insensitive},

    /* Buffer Search Tests */
    {"search_find_next_basic", test_search_find_next_basic},
    {"search_find_next_multiple_lines", test_search_find_next_multiple_lines},
    {"search_find_next_not_found", test_search_find_next_not_found},
    {"search_find_next_wrap_around", test_search_find_next_wrap_around},
    {"search_find_next_same_line_multiple", test_search_find_next_same_line_multiple},
    {"search_find_next_empty_buffer", test_search_find_next_empty_buffer},
    {"search_find_next_null_buffer", test_search_find_next_null_buffer},
    {"search_find_next_no_pattern", test_search_find_next_no_pattern},
    {"search_find_prev_basic", test_search_find_prev_basic},
    {"search_find_prev_multiple", test_search_find_prev_multiple},
    {"search_find_prev_wrap_around", test_search_find_prev_wrap_around},
    {"search_find_prev_same_line", test_search_find_prev_same_line},

    /* Replace Tests */
    {"search_replace_line_basic", test_search_replace_line_basic},
    {"search_replace_line_global", test_search_replace_line_global},
    {"search_replace_line_not_found", test_search_replace_line_not_found},
    {"search_replace_line_first_only", test_search_replace_line_first_only},
    {"search_replace_line_empty_replacement", test_search_replace_line_empty_replacement},
    {"search_replace_line_longer_replacement", test_search_replace_line_longer_replacement},
    {"search_replace_line_invalid_line", test_search_replace_line_invalid_line},
    {"search_replace_line_null_args", test_search_replace_line_null_args},
    {"search_replace_all_basic", test_search_replace_all_basic},
    {"search_replace_all_global", test_search_replace_all_global},
    {"search_replace_all_not_found", test_search_replace_all_not_found},
    {"search_replace_range_basic", test_search_replace_range_basic},
    {"search_replace_range_out_of_bounds", test_search_replace_range_out_of_bounds},
    {"search_replace_range_negative_start", test_search_replace_range_negative_start},
    {"search_replace_range_inverted", test_search_replace_range_inverted},

    {NULL, NULL}
};
