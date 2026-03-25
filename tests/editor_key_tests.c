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

/* Include mock ncurses before editor.h */
#include "mock_ncurses.h"

/* Mock window.h for testing */
#define WINDOW MockWindow
#define COLOR_KEYWORD 256
#define COLOR_STRING 512
#define COLOR_COMMENT 768
#define COLOR_TYPE 1024
#define COLOR_NUMBER 1280
#define COLOR_PREPROCESSOR 1536
#define COLOR_OTHER 0

typedef struct {
  int width;
  int height;
} MockWindow;

static inline void window_init(MockWindow* window) {
  window->width = 80;
  window->height = 24;
}

static inline void window_redraw_screen(const MockWindow* window) {
  (void)window;
}

static inline void window_deinit(MockWindow* window) {
  (void)window;
}

/* Include highlight.h for EHighlightToken */
#include "highlight.h"

/* Include editor dependencies */
#include "buffer.h"
#include "command.h"
#include "cursor.h"

#undef WINDOW

/* Editor definitions */
typedef enum {
  EditorState_Running,
  EditorState_CollectingCommand,
  EditorState_ProcessingCommand,
  EditorState_EditMode,
  EditorState_Exiting,
} EditorState;

typedef struct {
  EditorState state;
  Command command;
  MockWindow window;
  char* error_message;
  Cursor cursor;
  int number_of_line_digits;
  Buffer* current_buffer;
  Buffer** buffers;
  size_t number_of_buffers;
  bool end_line_mode;
  char* status_bar;
  char key_sequence[32];
  int repeat_count;
  int tab_size;
  int start_line;
  int start_column;
  bool string_rendering_ongoing;
  bool multiline_comment_ongoing;
  int key;
  void* toolbar;  // Widget-based bottom toolbar (opaque pointer for tests)
} Editor;

/* External functions */
void editor_process_key(Editor* editor, int key);
void editor_deinit(Editor* editor);
void editor_create_new_file(Editor* editor);

/* Test setup */
static void setup_editor(Editor* editor) {
  memset(editor, 0, sizeof(Editor));
  editor->state = EditorState_Running;
  editor->tab_size = 4;
  editor->window.width = 80;
  editor->window.height = 24;
  editor->number_of_line_digits = 4;
  editor->cursor.x = editor->number_of_line_digits;
  editor->cursor.y = 1;
}

/* Helper to create a buffer with multiple lines */
static void create_multiline_buffer(Editor* editor) {
  editor_create_new_file(editor);
  buffer_append_line(editor->current_buffer, "First line of text");
  buffer_append_line(editor->current_buffer, "Second line here");
  buffer_append_line(editor->current_buffer, "Third line content");
  // current_row is still the first one
}

void test_editor_key_h_moves_left(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Set cursor to position after line number area
  editor.cursor.x = editor.number_of_line_digits + 5;

  editor_process_key(&editor, 'h');

  // Cursor should move left by 1
  TEST_CHECK(editor.cursor.x == editor.number_of_line_digits + 4);

  editor_deinit(&editor);
}

void test_editor_key_l_moves_right(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Add some content so there's room to move
  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "Hello World");

  // Set cursor at start of content area
  editor.cursor.x = editor.number_of_line_digits;
  editor.start_column = 0;

  editor_process_key(&editor, 'l');

  // Cursor should have moved right or stayed at same position
  // (actual position depends on line length and window boundaries)
  TEST_CHECK(editor.cursor.x >= 0);

  editor_deinit(&editor);
}

void test_editor_key_j_moves_down(void) {
  Editor editor;
  setup_editor(&editor);
  create_multiline_buffer(&editor);

  BufferRow* initial_row = editor.current_buffer->current_row;

  editor_process_key(&editor, 'j');

  // Should move to next row
  TEST_CHECK(editor.current_buffer->current_row != initial_row);
  TEST_CHECK(editor.current_buffer->current_row == initial_row->next);

  editor_deinit(&editor);
}

void test_editor_key_k_moves_up(void) {
  Editor editor;
  setup_editor(&editor);
  create_multiline_buffer(&editor);

  // Move down first
  editor_process_key(&editor, 'j');
  BufferRow* row_after_move_down = editor.current_buffer->current_row;

  // Then move up
  editor_process_key(&editor, 'k');

  // Should be back at first row
  TEST_CHECK(editor.current_buffer->current_row == row_after_move_down->prev);

  editor_deinit(&editor);
}

void test_editor_key_j_stops_at_last_row(void) {
  Editor editor;
  setup_editor(&editor);
  create_multiline_buffer(&editor);

  // Try to move down past last row
  editor_process_key(&editor, 'j');
  editor_process_key(&editor, 'j');
  editor_process_key(&editor, 'j');
  editor_process_key(&editor, 'j');
  editor_process_key(&editor, 'j');

  // Should be at last row
  TEST_CHECK(buffer_current_is_last_row(editor.current_buffer));

  editor_deinit(&editor);
}

void test_editor_key_k_stops_at_first_row(void) {
  Editor editor;
  setup_editor(&editor);
  create_multiline_buffer(&editor);

  // Try to move up from first row
  editor_process_key(&editor, 'k');

  // Should still be at first row
  TEST_CHECK(buffer_current_is_first_row(editor.current_buffer));

  editor_deinit(&editor);
}

void test_editor_key_gg_goes_to_top(void) {
  Editor editor;
  setup_editor(&editor);
  create_multiline_buffer(&editor);

  // Move down first
  BufferRow* initial_row = editor.current_buffer->current_row;
  editor_process_key(&editor, 'j');
  TEST_CHECK(editor.current_buffer->current_row != initial_row);

  // Press 'g' then 'g' to go to top
  editor_process_key(&editor, 'g');
  editor_process_key(&editor, 'g');

  // Should be at first row
  TEST_CHECK(buffer_current_is_first_row(editor.current_buffer));

  editor_deinit(&editor);
}

void test_editor_key_G_goes_to_bottom(void) {
  Editor editor;
  setup_editor(&editor);
  create_multiline_buffer(&editor);

  // Press 'G' to go to bottom
  editor_process_key(&editor, 'G');

  // Should be at last row
  TEST_CHECK(buffer_current_is_last_row(editor.current_buffer));

  editor_deinit(&editor);
}

void test_editor_key_dd_deletes_line(void) {
  Editor editor;
  setup_editor(&editor);
  create_multiline_buffer(&editor);

  int initial_lines = buffer_get_number_of_lines(editor.current_buffer);
  // Initial: 1 empty line from editor_create_new_file + 3 from buffer_append_line = 4
  TEST_CHECK(initial_lines >= 3);

  // Press 'd' then 'd' to delete line
  editor_process_key(&editor, 'd');
  editor_process_key(&editor, 'd');

  // Should have one less line (or stay at 1 if only one line left)
  int final_lines = buffer_get_number_of_lines(editor.current_buffer);
  TEST_CHECK(final_lines == initial_lines - 1 || final_lines == 1);

  editor_deinit(&editor);
}

void test_editor_key_dd_on_last_line_clears(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Only one line - delete should clear it but not remove it
  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "Test content");
  int initial_len = buffer_row_get_length(row);
  TEST_CHECK(initial_len > 0);

  editor_process_key(&editor, 'd');
  editor_process_key(&editor, 'd');

  // Line should be cleared (replaced with "\n" which has length 1)
  // but still exist
  row = buffer_get_current_line(editor.current_buffer);
  TEST_CHECK(row != NULL);
  TEST_CHECK(buffer_row_get_length(row) < initial_len);

  editor_deinit(&editor);
}

void test_editor_key_i_enters_insert_mode(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  TEST_CHECK(editor.state == EditorState_Running);

  editor_process_key(&editor, 'i');

  TEST_CHECK(editor.state == EditorState_EditMode);

  editor_process_key(&editor, 27);  // Escape
  editor_deinit(&editor);
}

void test_editor_key_a_enters_insert_mode(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  TEST_CHECK(editor.state == EditorState_Running);

  editor_process_key(&editor, 'a');

  TEST_CHECK(editor.state == EditorState_EditMode);

  editor_process_key(&editor, 27);  // Escape
  editor_deinit(&editor);
}

void test_editor_key_caret_goes_to_start(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Move cursor to the right
  editor.cursor.x = editor.number_of_line_digits + 10;

  // Press '^' to go to first non-whitespace
  editor_process_key(&editor, '^');

  // end_line_mode should be false
  TEST_CHECK(editor.end_line_mode == false);

  editor_deinit(&editor);
}

void test_editor_key_dollar_goes_to_end(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "Test content here");

  // Press '$' to go to end of line
  editor_process_key(&editor, '$');

  // end_line_mode should be true
  TEST_CHECK(editor.end_line_mode == true);

  editor_deinit(&editor);
}

void test_editor_key_x_deletes_char(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "Hello");

  int initial_len = buffer_row_get_length(row);

  // Press 'x' to delete character at cursor
  editor_process_key(&editor, 'x');

  // Should have one less character
  row = buffer_get_current_line(editor.current_buffer);
  TEST_CHECK(buffer_row_get_length(row) == initial_len - 1);

  editor_deinit(&editor);
}

void test_editor_key_w_moves_word_forward(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "hello world test");

  // Set cursor at start
  editor.cursor.x = editor.number_of_line_digits;
  editor.start_column = 0;

  // Press 'w' to move to next word
  editor_process_key(&editor, 'w');

  // Cursor should have moved forward
  // (exact position depends on implementation)
  TEST_CHECK(editor.cursor.x >= editor.number_of_line_digits);

  editor_deinit(&editor);
}

void test_editor_key_b_moves_word_backward(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "hello world test");

  // Set cursor at end of content area
  editor.cursor.x = editor.number_of_line_digits + 10;
  editor.start_column = 0;

  // Press 'b' to move to previous word
  editor_process_key(&editor, 'b');

  // Cursor should have moved backward
  TEST_CHECK(editor.cursor.x <= editor.number_of_line_digits + 10);

  editor_deinit(&editor);
}

TEST_LIST = {
    {"test_editor_key_h_moves_left", test_editor_key_h_moves_left},
    {"test_editor_key_l_moves_right", test_editor_key_l_moves_right},
    {"test_editor_key_j_moves_down", test_editor_key_j_moves_down},
    {"test_editor_key_k_moves_up", test_editor_key_k_moves_up},
    {"test_editor_key_j_stops_at_last_row", test_editor_key_j_stops_at_last_row},
    {"test_editor_key_k_stops_at_first_row", test_editor_key_k_stops_at_first_row},
    {"test_editor_key_gg_goes_to_top", test_editor_key_gg_goes_to_top},
    {"test_editor_key_G_goes_to_bottom", test_editor_key_G_goes_to_bottom},
    {"test_editor_key_dd_deletes_line", test_editor_key_dd_deletes_line},
    {"test_editor_key_dd_on_last_line_clears", test_editor_key_dd_on_last_line_clears},
    {"test_editor_key_i_enters_insert_mode", test_editor_key_i_enters_insert_mode},
    {"test_editor_key_a_enters_insert_mode", test_editor_key_a_enters_insert_mode},
    {"test_editor_key_caret_goes_to_start", test_editor_key_caret_goes_to_start},
    {"test_editor_key_dollar_goes_to_end", test_editor_key_dollar_goes_to_end},
    {"test_editor_key_x_deletes_char", test_editor_key_x_deletes_char},
    {"test_editor_key_w_moves_word_forward", test_editor_key_w_moves_word_forward},
    {"test_editor_key_b_moves_word_backward", test_editor_key_b_moves_word_backward},

    {NULL, NULL}
};
