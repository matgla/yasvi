/*
 Copyright (c) 2026 Mateusz Stadnik

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

// :!cmd, :r !cmd and :r FILE: run.c's helpers and the commands through the
// editor, with the host's bash (MAKE_SHELL).

#define _GNU_SOURCE

#include "acutest.h"

#include "mock_ncurses.h"

#include "editor.h"
#include "run.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_dir[64];

static void make_temp_dir(void) {
  snprintf(g_dir, sizeof(g_dir), "/tmp/yasvi_shell_XXXXXX");
  TEST_ASSERT(mkdtemp(g_dir) != NULL);
}

static void remove_temp_dir(void) {
  char cmd[160];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
  TEST_CHECK(system(cmd) == 0);
}

static char* path_in_dir(const char* name) {
  static char paths[8][300];
  static int next;
  char* out = paths[next++ % 8];
  snprintf(out, 300, "%s/%s", g_dir, name);
  return out;
}

static void write_file(const char* path, const char* text) {
  FILE* f = fopen(path, "w");
  TEST_ASSERT(f != NULL);
  fputs(text, f);
  fclose(f);
}

static const char* read_file(const char* path) {
  static char text[8192];
  FILE* f = fopen(path, "r");
  if (f == NULL) {
    return NULL;
  }
  size_t n = fread(text, 1, sizeof(text) - 1, f);
  text[n] = '\0';
  fclose(f);
  return text;
}

static void check_string(const char* actual, const char* expected) {
  TEST_CHECK(actual != NULL && strcmp(actual, expected) == 0);
  TEST_MSG("got      [%s]", actual != NULL ? actual : "(null)");
  TEST_MSG("expected [%s]", expected);
}

static void stdin_from_dev_null(void) {
  int fd = open("/dev/null", O_RDONLY);
  TEST_ASSERT(fd >= 0);
  dup2(fd, STDIN_FILENO);
  close(fd);
}

static void setup_editor(Editor* editor) {
  memset(editor, 0, sizeof(*editor));
  editor->state = EditorState_Running;
  editor->tab_size = 4;
  editor->window.width = 80;
  editor->window.height = 24;
  editor->number_of_line_digits = 4;
  editor->cursor.x = editor->number_of_line_digits;
  editor->cursor.y = 2;
}

static void enter_command(Editor* editor, const char* cmd) {
  editor_process_key(editor, ':');
  for (size_t i = 0; i < strlen(cmd); i++) {
    editor_process_key(editor, cmd[i]);
  }
  editor_process_key(editor, '\n');
}

static const char* message(const Editor* editor) {
  return editor->error_message != NULL ? editor->error_message : "";
}

// The buffer's rows joined with \n.
static const char* buffer_text(const Editor* editor) {
  static char text[4096];
  size_t used = 0;
  text[0] = '\0';
  for (BufferRow* row = buffer_get_first_row(editor->current_buffer); row != NULL;
       row = row->next) {
    used += (size_t)snprintf(text + used, sizeof(text) - used, "%s\n", row->data);
  }
  return text;
}

static int current_line(const Editor* editor) {
  return buffer_get_current_line_number(editor->current_buffer);
}

/* ---- run.c ------------------------------------------------------------- */

void test_run_expand_command(void) {
  bool no_name = true;
  char* out = run_expand_command("ls -l", "a.c", &no_name);
  check_string(out, "ls -l");
  TEST_CHECK(!no_name);
  free(out);
  out = run_expand_command("tcc -c % -o %.o", "my file.c", NULL);
  check_string(out, "tcc -c 'my file.c' -o 'my file.c'.o");
  free(out);
  out = run_expand_command("printf 100\\%", NULL, &no_name);
  check_string(out, "printf 100%");
  TEST_CHECK(!no_name);
  free(out);
  TEST_CHECK(run_expand_command("wc %", NULL, &no_name) == NULL);
  TEST_CHECK(no_name);
}

void test_run_shell_to_file(void) {
  make_temp_dir();
  const char* out = path_in_dir("out");
  TEST_CHECK(run_shell_to_file("echo one; echo two >&2; read x || echo eof", out) == 0);
  check_string(read_file(out), "one\ntwo\neof\n");  // stdin is /dev/null
  TEST_CHECK(run_shell_to_file("exit 3", out) == 3);
  remove_temp_dir();
}

/* ---- through the editor ------------------------------------------------ */

void test_read_command_inserts_output_below(void) {
  make_temp_dir();
  write_file(path_in_dir("a.c"), "first\nlast\n");
  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, path_in_dir("a.c"));
  enter_command(&editor, "r !printf 'x\\ny\\n'; echo %");
  char expected[512];
  snprintf(expected, sizeof(expected), "first\nx\ny\n%s\nlast\n", path_in_dir("a.c"));
  check_string(buffer_text(&editor), expected);
  TEST_CHECK(current_line(&editor) == 2);  // on the first line read
  TEST_CHECK(buffer_is_modified(editor.current_buffer));
  TEST_CHECK(strstr(message(&editor), ": 3 lines, exit 0") != NULL);
  TEST_MSG("message: %s", message(&editor));
  TEST_CHECK(access(RUN_OUTPUT_PATH, F_OK) != 0);  // the temporary file is gone

  enter_command(&editor, "r !exit 2");
  TEST_CHECK(strstr(message(&editor), ": 0 lines, exit 2") != NULL);
  TEST_MSG("message: %s", message(&editor));
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_read_file_inserts_lines(void) {
  make_temp_dir();
  write_file(path_in_dir("a.c"), "one\ntwo\n");
  write_file(path_in_dir("b.txt"), "B1\nB2\n");
  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, path_in_dir("a.c"));
  char command[400];
  snprintf(command, sizeof(command), "r %s", path_in_dir("b.txt"));
  enter_command(&editor, command);
  check_string(buffer_text(&editor), "one\nB1\nB2\ntwo\n");
  snprintf(command, sizeof(command), "read %s", path_in_dir("missing"));
  enter_command(&editor, command);
  TEST_CHECK(strstr(message(&editor), "Cannot read") != NULL);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_bang_runs_command(void) {
  make_temp_dir();
  stdin_from_dev_null();  // "press Enter" reads end of input
  write_file(path_in_dir("a.c"), "x\n");
  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, path_in_dir("a.c"));
  enter_command(&editor, "!cp % %.copy");
  char copy[320];
  snprintf(copy, sizeof(copy), "%s.copy", path_in_dir("a.c"));
  check_string(read_file(copy), "x\n");
  TEST_CHECK(strstr(message(&editor), ": exit 0") != NULL);
  TEST_MSG("message: %s", message(&editor));
  enter_command(&editor, "!exit 5");
  TEST_CHECK(strstr(message(&editor), ": exit 5") != NULL);
  enter_command(&editor, "!");
  TEST_CHECK(strstr(message(&editor), "Usage: :!command") != NULL);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_percent_without_file_name(void) {
  stdin_from_dev_null();
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  enter_command(&editor, "!cat %");
  TEST_CHECK(strstr(message(&editor), "No file name for %") != NULL);
  TEST_MSG("message: %s", message(&editor));
  editor_deinit(&editor);
}

TEST_LIST = {
  {"run_expand_command", test_run_expand_command},
  {"run_shell_to_file", test_run_shell_to_file},
  {"read_command_inserts_output_below", test_read_command_inserts_output_below},
  {"read_file_inserts_lines", test_read_file_inserts_lines},
  {"bang_runs_command", test_bang_runs_command},
  {"percent_without_file_name", test_percent_without_file_name},
  {NULL, NULL}};
