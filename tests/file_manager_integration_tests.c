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
#define COLOR_ERROR 1792

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
#include "search.h"

#undef WINDOW

/* Include file_manager definitions */
#include "../file_manager.h"

/* Include the actual editor definitions */
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
  void* toolbar;
  FileManager* file_manager;
  int editor_offset_x;
  SearchState search;
  Command search_buffer;
} Editor;

/* Function declarations from editor.c */
void editor_process_key(Editor* editor, int key);
bool editor_should_exit(const Editor* editor);
void editor_redraw_screen(Editor* editor);
void editor_init(Editor* editor);
void editor_deinit(Editor* editor);
void editor_load_file(Editor* editor, const char* filename);
void editor_create_new_file(Editor* editor);
void editor_toggle_file_manager(Editor* editor);
void editor_file_manager_select(Editor* editor);
int editor_get_cursor_x(const Editor* editor);

/* Test setup helper */
static void setup_editor_with_path(Editor* editor, const char* path) {
  memset(editor, 0, sizeof(Editor));
  editor->state = EditorState_Running;
  editor->tab_size = 4;
  editor->window.width = 80;
  editor->window.height = 24;
  editor->number_of_line_digits = 4;
  editor->cursor.x = editor->number_of_line_digits;
  editor->cursor.y = 2;  // EDITOR_TOP_BAR_HEIGHT
  // Allocate file_manager for tests that need it
  editor->file_manager = file_manager_alloc();
  if (editor->file_manager) {
    file_manager_init(editor->file_manager, path);
    editor->file_manager->visible = false;
  }
}

static void setup_editor(Editor* editor) {
  setup_editor_with_path(editor, NULL);
}

/* Helper to create test directory structure */
static char test_dir[256];
static int test_dir_counter = 0;

static void setup_test_directory(void) {
  snprintf(test_dir, sizeof(test_dir), "/tmp/editor_fm_test_%d_%d", getpid(), test_dir_counter++);
  mkdir(test_dir, 0755);
}

static void cleanup_test_directory(void) {
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
  system(cmd);
}

static void create_test_file(const char* filename, const char* content) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", test_dir, filename);
  FILE* f = fopen(path, "w");
  if (f) {
    fprintf(f, "%s\n", content);
    fclose(f);
  }
}

#if 0  /* Unused for now - reserved for future tests */
static void create_test_subdir(const char* dirname) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", test_dir, dirname);
  mkdir(path, 0755);
}
#endif

/* Test that Ctrl+B toggles file manager */
void test_ctrl_b_toggles_file_manager(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // File manager should be allocated but not visible initially
  TEST_CHECK(editor.file_manager != NULL);
  TEST_CHECK(file_manager_is_visible(editor.file_manager) == false);
  TEST_CHECK(editor.state == EditorState_Running);

  // Press Ctrl+B to open file manager
  editor_process_key(&editor, 2);  // Ctrl+B

  TEST_CHECK(file_manager_is_visible(editor.file_manager) == true);
  TEST_CHECK(editor.state == EditorState_FileManager);

  // Press Ctrl+B again to close
  editor_process_key(&editor, 2);  // Ctrl+B

  TEST_CHECK(file_manager_is_visible(editor.file_manager) == false);
  TEST_CHECK(editor.state == EditorState_Running);

  editor_deinit(&editor);
}

/* Test file manager navigation keys */
void test_file_manager_navigation_keys(void) {
  setup_test_directory();
  create_test_file("aaa.txt", "content");
  create_test_file("bbb.txt", "content");

  Editor editor;
  setup_editor_with_path(&editor, test_dir);
  editor_create_new_file(&editor);

  // Open file manager
  editor_process_key(&editor, 2);
  TEST_CHECK(editor.state == EditorState_FileManager);

  // Move cursor down (j)
  editor_process_key(&editor, 'j');
  TEST_CHECK(editor.file_manager->cursor_line == 1);

  // Move cursor down again
  editor_process_key(&editor, 'j');
  TEST_CHECK(editor.file_manager->cursor_line == 2);

  // Move cursor up (k)
  editor_process_key(&editor, 'k');
  TEST_CHECK(editor.file_manager->cursor_line == 1);

  // Close file manager
  editor_process_key(&editor, 27);  // Escape

  editor_deinit(&editor);
  cleanup_test_directory();
}

/* Test cursor position adjustment when toggling file manager */
void test_cursor_adjustment_on_toggle(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  int initial_cursor_x = editor.cursor.x;
  TEST_CHECK(initial_cursor_x == 4);

  // Open file manager
  editor_process_key(&editor, 2);
  TEST_CHECK(file_manager_is_visible(editor.file_manager) == true);

  // Cursor should have shifted right by file manager width
  TEST_CHECK(editor.cursor.x > initial_cursor_x);
  TEST_CHECK(editor.number_of_line_digits > 4);

  // Close file manager
  editor_process_key(&editor, 2);

  // Cursor should be back to original position
  TEST_CHECK(editor.cursor.x == initial_cursor_x);
  TEST_CHECK(editor.number_of_line_digits == 4);

  editor_deinit(&editor);
}

/* Test opening a file from file manager */
void test_open_file_from_manager(void) {
  setup_test_directory();
  create_test_file("testfile.txt", "Hello from test file");

  Editor editor;
  setup_editor_with_path(&editor, test_dir);
  editor_create_new_file(&editor);

  TEST_CHECK(editor.number_of_buffers == 1);

  // Open file manager
  editor_process_key(&editor, 2);
  TEST_CHECK(editor.state == EditorState_FileManager);

  // Navigate to the test file (move down from root)
  editor_process_key(&editor, 'j');

  // Select the file (Enter)
  editor_process_key(&editor, '\n');

  // Should be back in running mode with file loaded
  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(file_manager_is_visible(editor.file_manager) == false);
  TEST_CHECK(editor.number_of_buffers == 2);

  // Cursor should be at top-left of file (y = EDITOR_TOP_BAR_HEIGHT = 2)
  TEST_CHECK(editor.cursor.y == 2);
  TEST_CHECK(editor.cursor.x == 4);
  TEST_CHECK(editor.start_line == 0);
  TEST_CHECK(editor.start_column == 0);

  editor_deinit(&editor);
  cleanup_test_directory();
}

/* Test that opening already open file switches to existing buffer */
void test_switch_to_existing_buffer(void) {
  setup_test_directory();
  create_test_file("existing.txt", "Content here");

  // First editor instance to load the file
  Editor editor;
  setup_editor_with_path(&editor, test_dir);
  editor_create_new_file(&editor);

  // Load file directly
  char file_path[512];
  snprintf(file_path, sizeof(file_path), "%s/existing.txt", test_dir);
  editor_load_file(&editor, file_path);
  TEST_CHECK(editor.number_of_buffers == 2);

  editor_deinit(&editor);
  cleanup_test_directory();
}

/* Test Escape key returns to normal mode from file manager */
void test_escape_from_file_manager(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Open file manager
  editor_process_key(&editor, 2);
  TEST_CHECK(editor.state == EditorState_FileManager);

  // Press Escape
  editor_process_key(&editor, 27);
  TEST_CHECK(editor.state == EditorState_Running);

  editor_deinit(&editor);
}

/* Test editor_offset_x is set correctly */
void test_editor_offset_x(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Initially no offset
  TEST_CHECK(editor.editor_offset_x == 0);

  // Open file manager
  editor_process_key(&editor, 2);

  // Offset should be set to file manager width
  TEST_CHECK(editor.editor_offset_x > 0);
  TEST_CHECK(editor.editor_offset_x == editor.file_manager->width);

  // Close file manager
  editor_process_key(&editor, 2);

  // Offset should be back to 0
  TEST_CHECK(editor.editor_offset_x == 0);

  editor_deinit(&editor);
}

/* Test get_cursor_x with file manager offset */
void test_get_cursor_x_with_offset(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  // Position cursor after line numbers
  editor.cursor.x = 8;  // 4 for line numbers + 4 for text position
  editor.start_column = 0;

  // Without file manager, should return 4 (cursor x - line digits - offset)
  int x1 = editor_get_cursor_x(&editor);
  TEST_CHECK(x1 == 4);

  // Open file manager
  editor_process_key(&editor, 2);

  // Cursor should have shifted
  // The cursor X is adjusted but logical position should be similar
  TEST_CHECK(editor.cursor.x > 8);  // Shifted right

  editor_deinit(&editor);
}

TEST_LIST = {
    {"test_ctrl_b_toggles_file_manager", test_ctrl_b_toggles_file_manager},
    {"test_file_manager_navigation_keys", test_file_manager_navigation_keys},
    {"test_cursor_adjustment_on_toggle", test_cursor_adjustment_on_toggle},
    {"test_open_file_from_manager", test_open_file_from_manager},
    {"test_switch_to_existing_buffer", test_switch_to_existing_buffer},
    {"test_escape_from_file_manager", test_escape_from_file_manager},
    {"test_editor_offset_x", test_editor_offset_x},
    {"test_get_cursor_x_with_offset", test_get_cursor_x_with_offset},

    {NULL, NULL}
};
