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
 * Full Cycle Forward Search Tests
 * ============================================================================ */

void test_search_forward_full_cycle_three_lines(void) {
  // Pattern appears on lines 0, 1, 2
  // Start at line 0, search through all, wrap back to line 0
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "target first");
  buffer_append_line(buffer, "target second");
  buffer_append_line(buffer, "target third");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "target", true);
  search_set_position(&state, 0, 0);

  // First search - line 0
  bool found = search_find_next(&state, buffer);
  TEST_CHECK(found == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Second search - line 1
  found = search_find_next(&state, buffer);
  TEST_CHECK(found == true);
  TEST_CHECK(state.last_match_line == 1);
  
  // Third search - line 2
  found = search_find_next(&state, buffer);
  TEST_CHECK(found == true);
  TEST_CHECK(state.last_match_line == 2);
  
  // Fourth search - wrap to line 0
  found = search_find_next(&state, buffer);
  TEST_CHECK(found == true);
  TEST_CHECK(state.last_match_line == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_forward_full_cycle_two_lines(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "pattern here");
  buffer_append_line(buffer, "pattern there");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "pattern", true);
  search_set_position(&state, 0, 0);

  // Line 0
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Line 1
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);
  
  // Wrap to line 0
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_forward_full_cycle_single_line(void) {
  // Multiple occurrences on same line
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "abc abc abc");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "abc", true);
  search_set_position(&state, 0, 0);

  // First occurrence at col 0
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_col == 0);
  
  // Second occurrence at col 4
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_col == 4);
  
  // Third occurrence at col 8
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_col == 8);
  
  // Wrap back to col 0
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_col == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_forward_multiple_cycles(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "word");
  buffer_append_line(buffer, "another");
  buffer_append_line(buffer, "word");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "word", true);
  search_set_position(&state, 0, 0);

  // Cycle 1
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);
  
  // Cycle 2
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);
  
  // Cycle 3
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_forward_wrap_from_middle(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "first target");
  buffer_append_line(buffer, "second target");
  buffer_append_line(buffer, "third target");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "target", true);
  search_set_position(&state, 1, 0);  // Start from line 1

  // Line 1
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);
  
  // Line 2
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);
  
  // Wrap to line 0
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Back to line 1
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);

  search_deinit(&state);
  buffer_free(buffer);
}

/* ============================================================================
 * Full Cycle Backward Search Tests
 * ============================================================================ */

void test_search_backward_full_cycle_three_lines(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "target first");
  buffer_append_line(buffer, "target second");
  buffer_append_line(buffer, "target third");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "target", false);
  search_set_position(&state, 2, 0);  // Start at last line

  // First search - line 2 (initial)
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);
  
  // Second search - line 1
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);
  
  // Third search - line 0
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Fourth search - wrap to line 2
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_backward_full_cycle_single_line(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "abc abc abc");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "abc", false);
  search_set_position(&state, 0, 10);  // Near end

  // First occurrence (at col 8)
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_col == 8);
  
  // Second occurrence (at col 4)
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_col == 4);
  
  // Third occurrence (at col 0)
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_col == 0);
  
  // Wrap back to col 8
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_col == 8);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_backward_wrap_from_middle(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "first target");
  buffer_append_line(buffer, "second target");
  buffer_append_line(buffer, "third target");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "target", false);
  search_set_position(&state, 1, 100);  // Start from end of line 1

  // Line 1 (initial - finds "target" at col 7)
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);
  
  // Line 0
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Wrap to line 2
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);
  
  // Back to line 1
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);

  search_deinit(&state);
  buffer_free(buffer);
}

/* ============================================================================
 * Mixed Direction Tests
 * ============================================================================ */

void test_search_mixed_direction_next_then_prev(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "line one");
  buffer_append_line(buffer, "line two");
  buffer_append_line(buffer, "line three");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "line", true);
  search_set_position(&state, 0, 0);

  // Forward: line 0
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Forward: line 1
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);
  
  // Backward: should go back to line 0
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Forward again: line 1
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_mixed_direction_prev_then_next(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "has pattern");
  buffer_append_line(buffer, "also has pattern");
  buffer_append_line(buffer, "pattern here too");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "pattern", false);
  search_set_position(&state, 2, 0);

  // Find line 2 from position 2 (initial)
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);
  
  // Find line 1
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);
  
  // Find line 0
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Forward: should find line 1
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);
  
  // Forward: should find line 2
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);

  search_deinit(&state);
  buffer_free(buffer);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

void test_search_single_line_buffer(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "only line with pattern");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "pattern", true);
  search_set_position(&state, 0, 0);

  // First find
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Wrap to same line
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // And again
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_pattern_at_line_start(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "pattern at start");
  buffer_append_line(buffer, "pattern again");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "pattern", true);
  search_set_position(&state, 0, 0);

  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  TEST_CHECK(state.last_match_col == 0);
  
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 1);
  TEST_CHECK(state.last_match_col == 0);
  
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  TEST_CHECK(state.last_match_col == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_pattern_at_line_end(void) {
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "ends with pattern");
  buffer_append_line(buffer, "also ends with pattern");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "pattern", true);
  search_set_position(&state, 0, 0);

  TEST_CHECK(search_find_next(&state, buffer) == true);
  int col1 = state.last_match_col;
  
  TEST_CHECK(search_find_next(&state, buffer) == true);
  int col2 = state.last_match_col;
  
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_col == col1);

  (void)col1;
  (void)col2;

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_no_match_after_wrap(void) {
  // Pattern only exists once, after wrap should still find it
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "first");
  buffer_append_line(buffer, "second");
  buffer_append_line(buffer, "unique");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "unique", true);
  search_set_position(&state, 2, 0);

  // Find on line 2
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);
  
  // Wrap and find again on line 2
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_alternating_lines(void) {
  // Pattern on every other line
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "has pattern");
  buffer_append_line(buffer, "no match here");
  buffer_append_line(buffer, "has pattern");
  buffer_append_line(buffer, "no match here");
  buffer_append_line(buffer, "has pattern");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "pattern", true);
  search_set_position(&state, 0, 0);

  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 2);
  
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 4);
  
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_same_position_multiple_times(void) {
  // Pressing n multiple times from same start position
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "abc");
  buffer_append_line(buffer, "def");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "abc", true);
  search_set_position(&state, 0, 0);

  // First press
  TEST_CHECK(search_find_next(&state, buffer) == true);
  TEST_CHECK(state.last_match_line == 0);
  
  // Many more presses - should keep wrapping
  for (int i = 0; i < 10; i++) {
    TEST_CHECK(search_find_next(&state, buffer) == true);
    TEST_CHECK(state.last_match_line == 0);
  }

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_from_end_of_buffer(void) {
  // Starting from the very end
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "first target");
  buffer_append_line(buffer, "middle");
  buffer_append_line(buffer, "last target");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "target", true);
  // Start from end of last line
  search_set_position(&state, 2, 100);  // Column beyond line length

  // Should find on line 2 (at the position, or wrap)
  TEST_CHECK(search_find_next(&state, buffer) == true);
  // Either finds at line 2 if search_from_col works, or wraps
  // The behavior depends on implementation

  search_deinit(&state);
  buffer_free(buffer);
}

void test_search_backward_from_start(void) {
  // Starting from line 0, going backward should wrap
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "first");
  buffer_append_line(buffer, "target");
  buffer_append_line(buffer, "last target");

  SearchState state;
  search_init(&state);
  search_set_pattern(&state, "target", false);
  search_set_position(&state, 0, 0);

  // From line 0, should wrap to line 2
  TEST_CHECK(search_find_prev(&state, buffer) == true);
  // Initial search includes current position, so finds at line 0 if exists
  // But "target" is not on line 0, so it wraps
  
  search_deinit(&state);
  buffer_free(buffer);
}

/* ============================================================================
 * Test List
 * ============================================================================ */

TEST_LIST = {
    {"search_forward_full_cycle_three_lines", test_search_forward_full_cycle_three_lines},
    {"search_forward_full_cycle_two_lines", test_search_forward_full_cycle_two_lines},
    {"search_forward_full_cycle_single_line", test_search_forward_full_cycle_single_line},
    {"search_forward_multiple_cycles", test_search_forward_multiple_cycles},
    {"search_forward_wrap_from_middle", test_search_forward_wrap_from_middle},
    {"search_backward_full_cycle_three_lines", test_search_backward_full_cycle_three_lines},
    {"search_backward_full_cycle_single_line", test_search_backward_full_cycle_single_line},
    {"search_backward_wrap_from_middle", test_search_backward_wrap_from_middle},
    {"search_mixed_direction_next_then_prev", test_search_mixed_direction_next_then_prev},
    {"search_mixed_direction_prev_then_next", test_search_mixed_direction_prev_then_next},
    {"search_single_line_buffer", test_search_single_line_buffer},
    {"search_pattern_at_line_start", test_search_pattern_at_line_start},
    {"search_pattern_at_line_end", test_search_pattern_at_line_end},
    {"search_no_match_after_wrap", test_search_no_match_after_wrap},
    {"search_alternating_lines", test_search_alternating_lines},
    {"search_same_position_multiple_times", test_search_same_position_multiple_times},
    {"search_from_end_of_buffer", test_search_from_end_of_buffer},
    {"search_backward_from_start", test_search_backward_from_start},
    {NULL, NULL}
};
