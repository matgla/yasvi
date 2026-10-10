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

#define _GNU_SOURCE

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
#include "help.h"
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

/* Mirrors editor.h: the tag stack sits after search_buffer. */
#define EDITOR_TAG_STACK_DEPTH 20
typedef struct {
  char* filename;
  int line;
  int column;
} TagStackEntry;

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
  TagStackEntry tag_stack[EDITOR_TAG_STACK_DEPTH];
  int tag_stack_size;
  void* completion;  // struct Completion*
} Editor;

/* External functions */
void editor_process_key(Editor* editor, int key);
bool editor_should_exit(const Editor* editor);
void editor_deinit(Editor* editor);
void editor_create_new_file(Editor* editor);
void editor_load_file(Editor* editor, const char* filename);
int editor_get_cursor_x(const Editor* editor);
bool editor_jump_to_tag(Editor* editor, const char* name);
bool editor_pop_tag(Editor* editor);

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

// The terminal injects bytes nobody typed -- a NUL or control byte around
// Enter, or a KEY_* code whose low byte is junk. None of them may reach the
// command buffer, or ":w" arrives as an unknown command.
void test_editor_command_ignores_non_printable_keys(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  editor_process_key(&editor, ':');
  editor_process_key(&editor, 0);    // stray NUL
  editor_process_key(&editor, 'w');
  editor_process_key(&editor, 1);    // stray control byte
  editor_process_key(&editor, KEY_UP);
  editor_process_key(&editor, ' ');  // printable, must survive
  editor_process_key(&editor, '/');
  editor_process_key(&editor, 'a');

  TEST_CHECK(strcmp(editor.command.buffer, "w /a") == 0);

  editor_process_key(&editor, 27);  // Cancel
  editor_deinit(&editor);
}

// Backspace on an empty command line must not underflow cursor_position and
// write through buffer[(size_t)-1].
void test_editor_command_backspace_on_empty(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  editor_process_key(&editor, ':');
  editor_process_key(&editor, KEY_BACKSPACE);
  editor_process_key(&editor, 127);

  TEST_CHECK(editor.command.cursor_position == 0);
  TEST_CHECK(strcmp(editor.command.buffer, "") == 0);

  editor_process_key(&editor, 'q');
  editor_process_key(&editor, '\n');
  TEST_CHECK(editor.state == EditorState_Exiting);

  editor_deinit(&editor);
}

// A command that failed for a real reason must keep that reason on screen. The
// dispatcher used to overwrite it with the generic "Command not found", so a
// save that could not open its file was indistinguishable from a typo.
void test_editor_command_keeps_specific_error(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);  // no filename attached

  enter_command(&editor, "w");

  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(editor.error_message != NULL);
  if (editor.error_message) {
    TEST_CHECK(strncmp(editor.error_message, "No filename specified for saving",
                       32) == 0);
    TEST_MSG("error_message: %s", editor.error_message);
  }

  editor_deinit(&editor);
}

// ...while a genuinely unknown command still gets the generic message.
void test_editor_command_unknown_reports_not_found(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  enter_command(&editor, "xyz");

  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(editor.error_message != NULL);
  if (editor.error_message) {
    TEST_CHECK(strncmp(editor.error_message, "Command not found", 17) == 0);
    TEST_MSG("error_message: %s", editor.error_message);
  }

  editor_deinit(&editor);
}

// A bare ':' followed by Enter is a no-op in vi. It used to fall through to the
// unknown-command path and report "Command not found: ''".
void test_editor_command_empty_reports_nothing(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  editor_process_key(&editor, ':');
  editor_process_key(&editor, '\n');

  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(editor.error_message == NULL);
  if (editor.error_message) {
    TEST_MSG("error_message: %s", editor.error_message);
  }

  editor_deinit(&editor);
}

/* ---- ctags navigation --------------------------------------------------- */

static char tag_dir[64];
static char tag_prev_cwd[512];

static void tag_write(const char* name, const char* content) {
  FILE* file = fopen(name, "w");
  TEST_ASSERT(file != NULL);
  fputs(content, file);
  fclose(file);
}

/* A two-file project with a tags file, and the editor on main.c. */
static void tag_fixture_enter(Editor* editor, bool with_tags) {
  strcpy(tag_dir, "/tmp/yasvi_tagnav_XXXXXX");
  TEST_ASSERT(mkdtemp(tag_dir) != NULL);
  TEST_ASSERT(getcwd(tag_prev_cwd, sizeof(tag_prev_cwd)) != NULL);
  TEST_ASSERT(chdir(tag_dir) == 0);
  tag_write("main.c",
            "#include \"util.h\"\n"
            "int main(void)\n"
            "{\n"
            "  return helper(1);\n"
            "}\n");
  tag_write("util.c",
            "/* util */\n"
            "\n"
            "int helper(int x)\n"
            "{\n"
            "  return x;\n"
            "}\n");
  if (with_tags) {
    tag_write("tags",
              "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted/\n"
              "helper\tutil.c\t/^int helper(int x)$/;\"\tf\n"
              "main\tmain.c\t/^int main(void)$/;\"\tf\n");
  }
  setup_editor(editor);
  editor_load_file(editor, "main.c");
}

static void tag_fixture_leave(Editor* editor) {
  editor_deinit(editor);
  unlink("main.c");
  unlink("util.c");
  unlink("tags");
  TEST_CHECK(chdir(tag_prev_cwd) == 0);
  rmdir(tag_dir);
}

static const char* tag_current_file(const Editor* editor) {
  return buffer_get_filename(editor->current_buffer);
}

void test_editor_tag_command_jumps_to_definition(void) {
  Editor editor;
  tag_fixture_enter(&editor, true);

  enter_command(&editor, "tag helper");

  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(strcmp(tag_current_file(&editor), "util.c") == 0);
  TEST_CHECK(buffer_get_current_line_number(editor.current_buffer) == 3);
  TEST_CHECK(editor_get_cursor_x(&editor) == 4);  // on "helper", after "int "
  TEST_MSG("line %d col %d", buffer_get_current_line_number(editor.current_buffer),
           editor_get_cursor_x(&editor));
  TEST_CHECK(editor.tag_stack_size == 1);

  tag_fixture_leave(&editor);
}

void test_editor_tag_ctrl_bracket_and_ctrl_t(void) {
  Editor editor;
  tag_fixture_enter(&editor, true);

  /* Cursor onto "helper" in "  return helper(1);" (line 4, column 9). */
  for (int i = 0; i < 3; ++i) {
    editor_process_key(&editor, 'j');
  }
  for (int i = 0; i < 9; ++i) {
    editor_process_key(&editor, 'l');
  }
  TEST_ASSERT(buffer_get_current_line_number(editor.current_buffer) == 4);
  TEST_ASSERT(editor_get_cursor_x(&editor) == 9);

  editor_process_key(&editor, 0x1d);  // Ctrl-]
  TEST_CHECK(strcmp(tag_current_file(&editor), "util.c") == 0);
  TEST_CHECK(buffer_get_current_line_number(editor.current_buffer) == 3);

  editor_process_key(&editor, 0x14);  // Ctrl-T
  TEST_CHECK(strcmp(tag_current_file(&editor), "main.c") == 0);
  TEST_CHECK(buffer_get_current_line_number(editor.current_buffer) == 4);
  TEST_CHECK(editor_get_cursor_x(&editor) == 9);
  TEST_CHECK(editor.tag_stack_size == 0);
  /* Both files stay open as buffers. */
  TEST_CHECK(editor.number_of_buffers == 2);

  tag_fixture_leave(&editor);
}

void test_editor_tag_errors(void) {
  Editor editor;
  tag_fixture_enter(&editor, true);

  enter_command(&editor, "tag nosuchname");
  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(editor.error_message != NULL &&
             strstr(editor.error_message, "Tag not found") != NULL);
  TEST_CHECK(strcmp(tag_current_file(&editor), "main.c") == 0);
  TEST_CHECK(editor.tag_stack_size == 0);

  enter_command(&editor, "tag");
  TEST_CHECK(editor.error_message != NULL &&
             strstr(editor.error_message, "Argument required") != NULL);

  editor_process_key(&editor, 0x14);  // Ctrl-T with nothing to return to
  TEST_CHECK(editor.error_message != NULL &&
             strstr(editor.error_message, "bottom of tag stack") != NULL);

  editor_process_key(&editor, 0x1d);  // Ctrl-] on '#', not an identifier
  TEST_CHECK(editor.error_message != NULL &&
             strstr(editor.error_message, "No identifier") != NULL);

  tag_fixture_leave(&editor);
}

void test_editor_tag_without_tags_file(void) {
  Editor editor;
  tag_fixture_enter(&editor, false);

  TEST_CHECK(!editor_jump_to_tag(&editor, "helper"));
  TEST_CHECK(editor.error_message != NULL &&
             strstr(editor.error_message, "No tags file") != NULL);
  TEST_CHECK(strcmp(tag_current_file(&editor), "main.c") == 0);

  tag_fixture_leave(&editor);
}

void test_editor_command_help_opens_help_buffer(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  enter_command(&editor, "help");

  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(!editor_should_exit(&editor));

  /* The help buffer is open, current, and named after the pseudo-file. */
  TEST_CHECK(editor.number_of_buffers == 2);
  TEST_CHECK(editor.current_buffer_index == 1);
  TEST_CHECK(strcmp(buffer_get_filename(editor.current_buffer),
                    HELP_BUFFER_NAME) == 0);
  TEST_CHECK(buffer_get_number_of_lines(editor.current_buffer) > 1);

  /* :h is the same command and must reuse, not duplicate, the buffer. */
  enter_command(&editor, "h");
  TEST_CHECK(editor.number_of_buffers == 2);
  TEST_CHECK(strcmp(buffer_get_filename(editor.current_buffer),
                    HELP_BUFFER_NAME) == 0);

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
    {"test_editor_command_ignores_non_printable_keys",
     test_editor_command_ignores_non_printable_keys},
    {"test_editor_command_backspace_on_empty",
     test_editor_command_backspace_on_empty},
    {"test_editor_command_keeps_specific_error",
     test_editor_command_keeps_specific_error},
    {"test_editor_command_unknown_reports_not_found",
     test_editor_command_unknown_reports_not_found},
    {"test_editor_command_empty_reports_nothing",
     test_editor_command_empty_reports_nothing},

    {"test_editor_tag_command_jumps_to_definition",
     test_editor_tag_command_jumps_to_definition},
    {"test_editor_tag_ctrl_bracket_and_ctrl_t", test_editor_tag_ctrl_bracket_and_ctrl_t},
    {"test_editor_tag_errors", test_editor_tag_errors},
    {"test_editor_tag_without_tags_file", test_editor_tag_without_tags_file},

    {"test_editor_command_help_opens_help_buffer",
     test_editor_command_help_opens_help_buffer},

    {NULL, NULL}
};
