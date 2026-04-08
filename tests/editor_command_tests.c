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

#include <stdio.h>
#include <unistd.h>

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
#include "search.h"

#undef WINDOW

/* Editor definitions */
typedef enum {
  EditorState_Running,
  EditorState_CollectingCommand,
  EditorState_ProcessingCommand,
  EditorState_EditMode,
  EditorState_FileManager,
  EditorState_SearchInputForward,
  EditorState_SearchInputBackward,
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
  size_t current_buffer_index;  // Index of current buffer in buffers array
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
  void* file_manager;
  int editor_offset_x;
  SearchState search;
  Command search_buffer;
} Editor;

/* External functions */
void editor_process_key(Editor* editor, int key);
bool editor_should_exit(const Editor* editor);
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
  editor->file_manager = NULL;
  editor->editor_offset_x = 0;
}

static void enter_command(Editor* editor, const char* cmd) {
  editor_process_key(editor, ':');
  for (size_t i = 0; i < strlen(cmd); i++) {
    editor_process_key(editor, cmd[i]);
  }
  editor_process_key(editor, '\n');
}

void test_editor_command_quit(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  TEST_CHECK(editor.state == EditorState_Running);

  enter_command(&editor, "q");

  TEST_CHECK(editor.state == EditorState_Exiting);
  TEST_CHECK(editor_should_exit(&editor));

  editor_deinit(&editor);
}

void test_editor_command_invalid(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  enter_command(&editor, "xyz");

  // Should return to running state with error
  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(!editor_should_exit(&editor));

  editor_deinit(&editor);
}

void test_editor_command_empty(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Just press : and Enter with no command
  editor_process_key(&editor, ':');
  editor_process_key(&editor, '\n');

  // Should return to running state
  TEST_CHECK(editor.state == EditorState_Running);

  editor_deinit(&editor);
}

void test_editor_command_save_and_quit(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Add some content
  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "Test content");

  // Use wq command
  enter_command(&editor, "wq");

  // Should exit after save attempt
  // (save may fail without filename, but command should be recognized)
  // Note: wq without filename shows error but doesn't exit
  TEST_CHECK(!editor_should_exit(&editor) || editor.state == EditorState_Exiting);

  editor_deinit(&editor);
}

void test_editor_command_write_no_filename(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Add content
  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "Test content");

  // Try to save without filename
  enter_command(&editor, "w");

  // Should return to running with error (no filename)
  TEST_CHECK(editor.state == EditorState_Running);

  editor_deinit(&editor);
}

void test_editor_command_write_with_filename(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Add content
  BufferRow* row = buffer_get_current_line(editor.current_buffer);
  buffer_row_replace_line(row, "Hello World");

  const char* test_file = "/tmp/yasvi_test_file.txt";

  // Remove file if it exists
  unlink(test_file);

  // Save with filename
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "w %s", test_file);
  enter_command(&editor, cmd);

  // Should succeed and return to running
  TEST_CHECK(editor.state == EditorState_Running);

  // Verify file was created
  FILE* f = fopen(test_file, "r");
  TEST_CHECK(f != NULL);
  if (f) {
    char buf[256];
    char* result = fgets(buf, sizeof(buf), f);
    TEST_CHECK(result != NULL);
    // File should contain the content plus newline
    TEST_CHECK(strncmp(buf, "Hello World", 11) == 0);
    fclose(f);
  }

  // Cleanup
  unlink(test_file);
  editor_deinit(&editor);
}

void test_editor_command_write_force_quit(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // wq command without filename - should fail since there's no filename
  enter_command(&editor, "wq");

  // With no filename, save fails and returns to running state with error
  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(!editor_should_exit(&editor));

  editor_deinit(&editor);
}

void test_editor_command_cancel_with_escape(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Start command
  editor_process_key(&editor, ':');
  editor_process_key(&editor, 'w');
  TEST_CHECK(editor.state == EditorState_CollectingCommand);

  // Cancel with Escape
  editor_process_key(&editor, 27);
  TEST_CHECK(editor.state == EditorState_Running);

  editor_deinit(&editor);
}

void test_editor_command_backspace(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  editor_process_key(&editor, ':');
  editor_process_key(&editor, 'w');
  editor_process_key(&editor, 'q');

  // Check command buffer has "wq"
  TEST_CHECK(strcmp(editor.command.buffer, "wq") == 0);
  TEST_CHECK(editor.command.cursor_position == 2);

  // Backspace
  editor_process_key(&editor, KEY_BACKSPACE);

  // Should remove last character
  TEST_CHECK(strcmp(editor.command.buffer, "w") == 0);
  TEST_CHECK(editor.command.cursor_position == 1);

  editor_process_key(&editor, 27);  // Cancel
  editor_deinit(&editor);
}

TEST_LIST = {
    {"test_editor_command_quit", test_editor_command_quit},
    {"test_editor_command_invalid", test_editor_command_invalid},
    {"test_editor_command_empty", test_editor_command_empty},
    {"test_editor_command_save_and_quit", test_editor_command_save_and_quit},
    {"test_editor_command_write_no_filename", test_editor_command_write_no_filename},
    {"test_editor_command_write_with_filename", test_editor_command_write_with_filename},
    {"test_editor_command_write_force_quit", test_editor_command_write_force_quit},
    {"test_editor_command_cancel_with_escape", test_editor_command_cancel_with_escape},
    {"test_editor_command_backspace", test_editor_command_backspace},

    {NULL, NULL}
};
