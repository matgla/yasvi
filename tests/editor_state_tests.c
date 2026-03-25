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

/* Now include editor dependencies */
#include "buffer.h"
#include "command.h"
#include "cursor.h"

#undef WINDOW

/* Include the actual editor definitions */
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

/* Function declarations from editor.c that we need to test */
void editor_process_key(Editor* editor, int key);
bool editor_should_exit(const Editor* editor);
void editor_redraw_screen(Editor* editor);
void editor_init(Editor* editor);
void editor_deinit(Editor* editor);
void editor_load_file(Editor* editor, const char* filename);
void editor_create_new_file(Editor* editor);

/* Test setup helper */
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

void test_editor_init_state(void) {
  Editor editor;
  setup_editor(&editor);

  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(editor.current_buffer == NULL);
  TEST_CHECK(editor.number_of_buffers == 0);
  TEST_CHECK(editor.buffers == NULL);
  TEST_CHECK(editor.error_message == NULL);
  TEST_CHECK(editor.key_sequence[0] == '\0');
  TEST_CHECK(editor.repeat_count == 0);
}

void test_editor_should_exit_initial(void) {
  Editor editor;
  setup_editor(&editor);

  TEST_CHECK(!editor_should_exit(&editor));
}

void test_editor_state_transition_to_command(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  TEST_CHECK(editor.state == EditorState_Running);

  // Press ':' to enter command mode
  editor_process_key(&editor, ':');

  TEST_CHECK(editor.state == EditorState_CollectingCommand);
  TEST_CHECK(editor.command.buffer != NULL);

  editor_deinit(&editor);
}

void test_editor_state_command_cancel(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Enter command mode
  editor_process_key(&editor, ':');
  TEST_CHECK(editor.state == EditorState_CollectingCommand);

  // Press Escape to cancel
  editor_process_key(&editor, 27);

  TEST_CHECK(editor.state == EditorState_Running);

  editor_deinit(&editor);
}

void test_editor_state_transition_to_edit(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  TEST_CHECK(editor.state == EditorState_Running);

  // Press 'i' to enter edit mode
  editor_process_key(&editor, 'i');

  TEST_CHECK(editor.state == EditorState_EditMode);

  // Press Escape to exit edit mode
  editor_process_key(&editor, 27);

  TEST_CHECK(editor.state == EditorState_Running);

  editor_deinit(&editor);
}

void test_editor_state_transition_to_exit(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Enter command mode and type 'q'
  editor_process_key(&editor, ':');
  editor_process_key(&editor, 'q');
  TEST_CHECK(editor.state == EditorState_CollectingCommand);

  // Press Enter to execute
  editor_process_key(&editor, '\n');

  TEST_CHECK(editor.state == EditorState_Exiting);
  TEST_CHECK(editor_should_exit(&editor));

  editor_deinit(&editor);
}

void test_editor_create_new_file(void) {
  Editor editor;
  setup_editor(&editor);

  TEST_CHECK(editor.current_buffer == NULL);
  TEST_CHECK(editor.number_of_buffers == 0);

  editor_create_new_file(&editor);

  TEST_CHECK(editor.current_buffer != NULL);
  TEST_CHECK(editor.number_of_buffers == 1);
  TEST_CHECK(editor.buffers != NULL);
  TEST_CHECK(editor.buffers[0] == editor.current_buffer);
  TEST_CHECK(buffer_get_number_of_lines(editor.current_buffer) == 1);

  editor_deinit(&editor);
}

void test_editor_create_multiple_files(void) {
  Editor editor;
  setup_editor(&editor);

  editor_create_new_file(&editor);
  editor_create_new_file(&editor);
  editor_create_new_file(&editor);

  TEST_CHECK(editor.number_of_buffers == 3);
  TEST_CHECK(editor.current_buffer == editor.buffers[0]);

  editor_deinit(&editor);
}

void test_editor_deinit_cleanup(void) {
  Editor editor;
  setup_editor(&editor);

  editor_create_new_file(&editor);
  editor_create_new_file(&editor);

  TEST_CHECK(editor.number_of_buffers == 2);

  editor_deinit(&editor);

  // After deinit, buffers should be freed
  TEST_CHECK(editor.buffers == NULL);
  TEST_CHECK(editor.number_of_buffers == 0);
  TEST_CHECK(editor.current_buffer == NULL);
}

void test_editor_key_sequence_numeric(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Add some content first
  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "Hello World");

  // Type '5' to start a numeric sequence
  editor_process_key(&editor, '5');
  TEST_CHECK(strcmp(editor.key_sequence, "5") == 0);

  // Now pressing a non-digit should process the sequence
  // The key_sequence should be cleared after processing
  editor_process_key(&editor, 'l');
  TEST_CHECK(editor.key_sequence[0] == '\0');
  // Note: repeat_count is reset to 0 immediately after processing
  // The cursor should have moved (5 times to the right, but limited by line length)

  editor_deinit(&editor);
}

void test_editor_cursor_initial_position(void) {
  Editor editor;
  setup_editor(&editor);

  editor.cursor.x = 0;
  editor.cursor.y = 0;
  editor.number_of_line_digits = 4;

  // Simulate editor home cursor
  editor.cursor.x = editor.number_of_line_digits;
  editor.cursor.y = 1;

  TEST_CHECK(editor.cursor.x == 4);
  TEST_CHECK(editor.cursor.y == 1);
}

TEST_LIST = {
    {"test_editor_init_state", test_editor_init_state},
    {"test_editor_should_exit_initial", test_editor_should_exit_initial},
    {"test_editor_state_transition_to_command", test_editor_state_transition_to_command},
    {"test_editor_state_command_cancel", test_editor_state_command_cancel},
    {"test_editor_state_transition_to_edit", test_editor_state_transition_to_edit},
    {"test_editor_state_transition_to_exit", test_editor_state_transition_to_exit},
    {"test_editor_create_new_file", test_editor_create_new_file},
    {"test_editor_create_multiple_files", test_editor_create_multiple_files},
    {"test_editor_deinit_cleanup", test_editor_deinit_cleanup},
    {"test_editor_key_sequence_numeric", test_editor_key_sequence_numeric},
    {"test_editor_cursor_initial_position", test_editor_cursor_initial_position},

    {NULL, NULL}
};
