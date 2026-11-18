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

#define _GNU_SOURCE

#include "acutest.h"

#include "mock_ncurses.h"

#include "editor.h"
#include "make.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- helpers ---------------------------------------------------------- */

static char g_dir[64];

static void make_temp_dir(void) {
  snprintf(g_dir, sizeof(g_dir), "/tmp/yasvi_make_XXXXXX");
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

static void type_text(Editor* editor, const char* text) {
  editor_process_key(editor, 'i');
  for (const char* p = text; *p; ++p) {
    editor_process_key(editor, *p);
  }
  editor_process_key(editor, 27);
}

static int current_line(const Editor* editor) {
  return buffer_get_current_line_number(editor->current_buffer);
}

static const char* message(const Editor* editor) {
  return editor->error_message != NULL ? editor->error_message : "";
}

/* ---- settings --------------------------------------------------------- */

void test_make_config_set(void) {
  MakeConfig config;
  make_config_init(&config);
  TEST_CHECK(config.makeprg == NULL);
  TEST_CHECK(config.mode == MakeMode_Wait);

  TEST_CHECK(make_config_set(&config, "makeprg=make -j4") == MakeSet_Ok);
  check_string(config.makeprg, "make -j4");
  TEST_CHECK(make_config_set(&config, "set makeprg = tcc a.c  \r\n") == MakeSet_Ok);
  check_string(config.makeprg, "tcc a.c");
  TEST_CHECK(make_config_set(&config, "  set makemode=exec\n") == MakeSet_Ok);
  TEST_CHECK(config.mode == MakeMode_Exec);
  TEST_CHECK(make_config_set(&config, "makemode=wait") == MakeSet_Ok);
  TEST_CHECK(config.mode == MakeMode_Wait);

  TEST_CHECK(make_config_set(&config, "") == MakeSet_Ignored);
  TEST_CHECK(make_config_set(&config, "   \n") == MakeSet_Ignored);
  TEST_CHECK(make_config_set(&config, "# makeprg=nope") == MakeSet_Ignored);
  check_string(config.makeprg, "tcc a.c");
  TEST_CHECK(make_config_set(&config, "makemode=fast") == MakeSet_BadValue);
  TEST_CHECK(config.mode == MakeMode_Wait);
  TEST_CHECK(make_config_set(&config, "tabstop=4") == MakeSet_UnknownKey);
  TEST_CHECK(make_config_set(&config, "makeprgx=1") == MakeSet_UnknownKey);
  TEST_CHECK(make_config_set(&config, "makeprg") == MakeSet_Query);
  TEST_CHECK(make_config_set(&config, "set makemode") == MakeSet_Query);

  // An empty value is the default again.
  TEST_CHECK(make_config_set(&config, "makeprg=") == MakeSet_Ok);
  TEST_CHECK(config.makeprg == NULL);
  make_config_deinit(&config);
}

void test_make_config_load_file_counts_bad_lines(void) {
  make_temp_dir();
  write_file(path_in_dir("rc"),
             "# yasvi settings\n"
             "\n"
             "makeprg=zig build -Doptimize=ReleaseSmall\n"
             "set makemode=exec\n"
             "bogus\n");
  MakeConfig config;
  make_config_init(&config);
  int bad = -1;
  TEST_CHECK(make_config_load_file(&config, path_in_dir("rc"), &bad));
  TEST_CHECK(bad == 1);
  check_string(config.makeprg, "zig build -Doptimize=ReleaseSmall");
  TEST_CHECK(config.mode == MakeMode_Exec);
  TEST_CHECK(!make_config_load_file(&config, path_in_dir("missing"), NULL));
  make_config_deinit(&config);
  remove_temp_dir();
}

void test_make_config_load_default_prefers_working_dir(void) {
  make_temp_dir();
  TEST_ASSERT(mkdir(path_in_dir("work"), 0755) == 0);
  TEST_ASSERT(mkdir(path_in_dir("home"), 0755) == 0);
  write_file(path_in_dir("home/.yasvirc"), "makeprg=from-home\n");
  MakeConfig config;
  make_config_init(&config);

  char* used = make_config_load_default(&config, path_in_dir("work"),
                                        path_in_dir("home"), NULL);
  check_string(used, path_in_dir("home/.yasvirc"));
  check_string(config.makeprg, "from-home");
  free(used);

  write_file(path_in_dir("work/.yasvirc"), "makeprg=from-work\n");
  used = make_config_load_default(&config, path_in_dir("work"), path_in_dir("home"),
                                  NULL);
  check_string(used, path_in_dir("work/.yasvirc"));
  check_string(config.makeprg, "from-work");
  free(used);

  TEST_CHECK(
    make_config_load_default(&config, path_in_dir("home/none"), NULL, NULL) == NULL);
  make_config_deinit(&config);
  remove_temp_dir();
}

void test_make_resolve_program(void) {
  make_temp_dir();
  MakeConfig config;
  make_config_init(&config);
  TEST_CHECK(make_resolve_program(&config, g_dir) == NULL);

  write_file(path_in_dir("Makefile"), "all:\n");
  char* program = make_resolve_program(&config, g_dir);
  check_string(program, "make");
  free(program);

  write_file(path_in_dir("build.sh"), "echo hi\n");
  program = make_resolve_program(&config, g_dir);
  check_string(program, "/usr/bin/bash ./build.sh");
  free(program);

  make_config_set(&config, "makeprg=tcc -run x.c");
  program = make_resolve_program(&config, g_dir);
  check_string(program, "tcc -run x.c");
  free(program);

  char text[160];
  make_config_describe(&config, g_dir, text, sizeof(text));
  check_string(text, "makeprg=tcc -run x.c makemode=wait");
  make_config_set(&config, "makeprg=");
  make_config_set(&config, "makemode=exec");
  make_config_describe(&config, g_dir, text, sizeof(text));
  check_string(text, "makeprg= (default: /usr/bin/bash ./build.sh) makemode=exec");
  make_config_deinit(&config);
  remove_temp_dir();
}

/* ---- scripts ---------------------------------------------------------- */

void test_make_build_script(void) {
  char* script = make_build_script("make", NULL, "/tmp/yasvi-make.log");
  check_string(script, ": > '/tmp/yasvi-make.log.rc'; { make; echo $? > "
                       "'/tmp/yasvi-make.log.rc'; } 2>&1 | tee '/tmp/yasvi-make.log'");
  free(script);
  script = make_build_script("make", "  -j2 all ", "/tmp/l");
  check_string(script, ": > '/tmp/l.rc'; { make -j2 all; echo $? > '/tmp/l.rc'; } "
                       "2>&1 | tee '/tmp/l'");
  free(script);
  script = make_build_script("make", "   ", "/tmp/l");
  check_string(script, ": > '/tmp/l.rc'; { make; echo $? > '/tmp/l.rc'; } "
                       "2>&1 | tee '/tmp/l'");
  free(script);
}

void test_make_build_exec_script(void) {
  char* script = make_build_exec_script("make", "test", "/tmp/yasvi-make.log",
                                        "/usr/bin/vi", "/tmp/yasvi-session");
  check_string(script,
               ": > '/tmp/yasvi-make.log.rc'; { make test; echo $? > "
               "'/tmp/yasvi-make.log.rc'; } 2>&1 | tee '/tmp/yasvi-make.log'; "
               "read -r rc < '/tmp/yasvi-make.log.rc'; echo; "
               "echo \"[build exited $rc] press Enter\"; read -r _; "
               "exec '/usr/bin/vi' -S '/tmp/yasvi-session'");
  free(script);
}

void test_make_shell_quote(void) {
  char* quoted = make_shell_quote("it's");
  check_string(quoted, "'it'\\''s'");
  free(quoted);
  quoted = make_shell_quote("");
  check_string(quoted, "''");
  free(quoted);
}

void test_make_exec_script_runs_in_bash(void) {
  // The exec script with `true` in place of vi: the status lands in LOG.rc.
  make_temp_dir();
  char log[320];
  snprintf(log, sizeof(log), "%s/log", g_dir);
  char* script = make_build_exec_script("/usr/bin/bash -c 'echo out; exit 3'", NULL,
                                        log, "/bin/true", "/dev/null");
  TEST_ASSERT(script != NULL);
  char* quoted = make_shell_quote(script);
  char cmd[1200];
  snprintf(cmd, sizeof(cmd), "echo | /usr/bin/bash -c %s > /dev/null", quoted);
  TEST_CHECK(system(cmd) == 0);
  free(quoted);
  free(script);
  TEST_CHECK(make_read_status(log) == 3);  // the build's status, not tee's
  check_string(read_file(log), "out\n");
  remove_temp_dir();
}

void test_make_read_status(void) {
  make_temp_dir();
  char log[320];
  snprintf(log, sizeof(log), "%s/log", g_dir);
  TEST_CHECK(make_read_status(log) == -1);
  write_file(path_in_dir("log.rc"), "2\n");
  TEST_CHECK(make_read_status(log) == 2);
  write_file(path_in_dir("log.rc"), "junk\n");
  TEST_CHECK(make_read_status(log) == -1);
  remove_temp_dir();
}

/* ---- error lines ------------------------------------------------------ */

static void check_error(const char* line,
                        const char* filename,
                        int line_number,
                        int column,
                        bool is_warning,
                        const char* text) {
  MakeError error;
  TEST_CHECK(make_parse_error_line(line, &error));
  TEST_MSG("line: %s", line);
  if (!TEST_CHECK(error.filename != NULL)) {
    return;
  }
  check_string(error.filename, filename);
  TEST_CHECK(error.line == line_number);
  TEST_CHECK(error.column == column);
  TEST_CHECK(error.is_warning == is_warning);
  check_string(error.message, text);
  make_error_clear(&error);
}

void test_make_parse_error_lines(void) {
  check_error("main.c:12: error: 'x' undeclared\n", "main.c", 12, 0, false,
              "'x' undeclared");
  check_error("src/main.zig:12:5: error: expected ';'\n", "src/main.zig", 12, 5,
              false, "expected ';'");
  check_error("a.c:3:1: warning: unused variable 'y' [-Wunused]", "a.c", 3, 1, true,
              "unused variable 'y' [-Wunused]");
  check_error("x.c:7: warning: implicit declaration", "x.c", 7, 0, true,
              "implicit declaration");
  check_error("lib/x.c:1:10: fatal error: y.h: No such file", "lib/x.c", 1, 10,
              false, "y.h: No such file");
  check_error("  b.c:4: error: indented", "b.c", 4, 0, false, "indented");
  check_error("c:\\x.c:4: error: colon in the name", "c:\\x.c", 4, 0, false,
              "colon in the name");

  MakeError error;
  TEST_CHECK(!make_parse_error_line("a.c:3:5: note: declared here", &error));
  TEST_CHECK(!make_parse_error_line("In file included from a.h:3:", &error));
  TEST_CHECK(!make_parse_error_line("make: *** [Makefile:12: all] Error 1", &error));
  TEST_CHECK(!make_parse_error_line("tcc: error: file 'x.c' not found", &error));
  TEST_CHECK(!make_parse_error_line("error: no file", &error));
  TEST_CHECK(!make_parse_error_line(":3: error: no file name", &error));
  TEST_CHECK(!make_parse_error_line("", &error));
}

void test_make_parse_log(void) {
  make_temp_dir();
  write_file(path_in_dir("log"),
             "tcc -c a.c\n"
             "a.c:3: warning: assignment makes pointer\n"
             "a.c:9: error: 'q' undeclared\n"
             "b.zig:2:7: error: expected type\n"
             "make: *** [Makefile:4: all] Error 1\n");
  MakeErrorList list = {NULL, 0, -1, 0};
  TEST_CHECK(make_parse_log(path_in_dir("log"), &list));
  TEST_CHECK(list.count == 3);
  TEST_CHECK(list.errors == 2);
  TEST_CHECK(list.current == -1);
  char text[128];
  make_error_format(&list.items[2], text, sizeof(text));
  check_string(text, "b.zig:2:7: error: expected type");
  make_error_format(&list.items[0], text, sizeof(text));
  check_string(text, "a.c:3: warning: assignment makes pointer");

  TEST_CHECK(!make_parse_log(path_in_dir("missing"), &list));
  TEST_CHECK(list.count == 0);
  make_error_list_clear(&list);
  remove_temp_dir();
}

/* ---- session ---------------------------------------------------------- */

void test_make_session_round_trip(void) {
  make_temp_dir();
  MakeSession session = {NULL, 0, 0};
  TEST_CHECK(make_session_add(&session, "main.c", 41, 3, false));
  TEST_CHECK(make_session_add(&session, "dir with spaces/x.zig", 0, 0, false));
  TEST_CHECK(make_session_add(&session, "/usr/share/man/printf.txt", 5, 0, true));
  session.current = 1;
  MakeConfig config;
  make_config_init(&config);
  make_config_set(&config, "makeprg=make -C sub all");
  make_config_set(&config, "makemode=exec");
  TEST_CHECK(make_session_write(path_in_dir("session"), &session, &config));
  make_session_clear(&session);
  make_config_deinit(&config);

  MakeSession back = {NULL, 0, 0};
  MakeConfig back_config;
  make_config_init(&back_config);
  TEST_CHECK(make_session_read(path_in_dir("session"), &back, &back_config));
  TEST_CHECK(back.count == 3);
  TEST_CHECK(back.current == 1);
  check_string(back.files[0].path, "main.c");
  TEST_CHECK(back.files[0].line == 41);
  TEST_CHECK(back.files[0].column == 3);
  TEST_CHECK(!back.files[0].autosave_disabled);
  check_string(back.files[1].path, "dir with spaces/x.zig");
  TEST_CHECK(back.files[2].autosave_disabled);
  TEST_CHECK(back.files[2].line == 5);
  check_string(back_config.makeprg, "make -C sub all");
  TEST_CHECK(back_config.mode == MakeMode_Exec);
  make_session_clear(&back);
  make_config_deinit(&back_config);

  write_file(path_in_dir("not-a-session"), "file 1 2 0 x.c\n");
  TEST_CHECK(!make_session_read(path_in_dir("not-a-session"), &back, NULL));
  TEST_CHECK(!make_session_read(path_in_dir("missing"), &back, NULL));
  TEST_CHECK(back.count == 0);
  remove_temp_dir();
}

/* ---- the editor: :set, the error list, -S, :make ---------------------- */

void test_make_set_command(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  enter_command(&editor, "set makeprg=tcc -o x x.c");
  TEST_CHECK(editor.make != NULL);
  check_string(editor.make->config.makeprg, "tcc -o x x.c");
  check_string(message(&editor), "norecord noautoindent makeprg=tcc -o x x.c makemode=wait");
  enter_command(&editor, "set makemode=exec");
  TEST_CHECK(editor.make->config.mode == MakeMode_Exec);
  enter_command(&editor, "set");
  check_string(message(&editor), "norecord noautoindent makeprg=tcc -o x x.c makemode=exec");
  enter_command(&editor, "set makemode=later");
  TEST_CHECK(strstr(message(&editor), "makemode is wait or exec") != NULL);
  TEST_CHECK(editor.make->config.mode == MakeMode_Exec);
  enter_command(&editor, "set number");
  TEST_CHECK(strstr(message(&editor), "Unknown option") != NULL);
  editor_deinit(&editor);
}

// Two files and a log naming them, for the error list.
static void write_build_fixture(void) {
  write_file(path_in_dir("a.c"), "int a;\nint b;\nint c;\nint d;\n");
  write_file(path_in_dir("b.c"), "x\ny\n");
  char log[1024];
  snprintf(log, sizeof(log),
           "%s:2: warning: first, a warning\n"
           "%s:3:5: error: c is wrong\n"
           "%s:2:1: error: y is wrong\n",
           path_in_dir("a.c"), path_in_dir("a.c"), path_in_dir("b.c"));
  write_file(path_in_dir("log"), log);
}

void test_make_report_jumps_to_first_error(void) {
  make_temp_dir();
  write_build_fixture();
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  editor_make_report(&editor, path_in_dir("log"), 1);
  check_string(buffer_get_filename(editor.current_buffer), path_in_dir("a.c"));
  TEST_CHECK(current_line(&editor) == 3);
  TEST_CHECK(editor_get_cursor_x(&editor) == 4);
  TEST_CHECK(editor.make->errors.current == 1);  // the warning is skipped
  TEST_CHECK(strncmp(message(&editor),
                     "build failed (exit 1), 2 errors, 1 warning: ", 44) == 0);
  TEST_MSG("message: %s", message(&editor));
  TEST_CHECK(strstr(message(&editor), "c is wrong") != NULL);

  enter_command(&editor, "cn");
  check_string(buffer_get_filename(editor.current_buffer), path_in_dir("b.c"));
  TEST_CHECK(current_line(&editor) == 2);
  TEST_CHECK(strstr(message(&editor), "(3 of 3)") != NULL);
  TEST_CHECK(strstr(message(&editor), "y is wrong") != NULL);
  enter_command(&editor, "cn");
  check_string(message(&editor), "No more errors");

  enter_command(&editor, "cp");
  check_string(buffer_get_filename(editor.current_buffer), path_in_dir("a.c"));
  TEST_CHECK(current_line(&editor) == 3);
  enter_command(&editor, "cp");
  TEST_CHECK(current_line(&editor) == 2);  // the warning
  TEST_CHECK(strstr(message(&editor), "warning: first, a warning") != NULL);
  enter_command(&editor, "cp");
  check_string(message(&editor), "Already at the first error");
  enter_command(&editor, "cc 3");
  check_string(buffer_get_filename(editor.current_buffer), path_in_dir("b.c"));
  enter_command(&editor, "cc");
  TEST_CHECK(strstr(message(&editor), "(3 of 3)") != NULL);
  TEST_CHECK(editor.number_of_buffers == 3);  // a.c and b.c, each opened once

  enter_command(&editor, "cl");
  check_string(buffer_get_filename(editor.current_buffer), path_in_dir("log"));
  TEST_CHECK(editor.current_buffer->autosave_disabled);
  // The next build's log replaces the text of the open log buffer.
  write_file(path_in_dir("log"), "all good\n");
  editor_switch_to_buffer_by_index(&editor, 0);
  enter_command(&editor, "cl");
  TEST_CHECK(editor.number_of_buffers == 4);
  check_string(buffer_get_first_row(editor.current_buffer)->data, "all good");
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_make_report_ok_and_no_errors(void) {
  make_temp_dir();
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  write_file(path_in_dir("log"), "compiling\ndone\n");
  editor_make_report(&editor, path_in_dir("log"), 0);
  check_string(message(&editor), "build ok");
  TEST_CHECK(editor.number_of_buffers == 1);
  enter_command(&editor, "cn");
  check_string(message(&editor), "No errors (run :make)");
  editor_make_report(&editor, path_in_dir("log"), 2);
  check_string(message(&editor), "build failed (exit 2), 0 errors");
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_make_restore_session(void) {
  make_temp_dir();
  write_build_fixture();
  // What makemode=exec leaves: the session, the log and its status.
  MakeSession session = {NULL, 0, 0};
  make_session_add(&session, path_in_dir("b.c"), 1, 0, false);
  make_session_add(&session, path_in_dir("a.c"), 3, 2, false);
  session.current = 1;
  MakeConfig config;
  make_config_init(&config);
  make_config_set(&config, "makeprg=./go.sh");
  make_config_set(&config, "makemode=exec");
  TEST_ASSERT(make_session_write(path_in_dir("session"), &session, &config));
  make_session_clear(&session);
  make_config_deinit(&config);
  write_file(MAKE_LOG_PATH, "everything compiled\n");
  write_file(MAKE_LOG_PATH MAKE_RC_SUFFIX, "0\n");

  Editor editor;
  setup_editor(&editor);
  TEST_CHECK(editor_restore_session(&editor, path_in_dir("session")));
  TEST_CHECK(editor.number_of_buffers == 2);
  check_string(buffer_get_filename(editor.current_buffer), path_in_dir("a.c"));
  TEST_CHECK(current_line(&editor) == 4);
  TEST_CHECK(editor_get_cursor_x(&editor) == 2);
  check_string(editor.make->config.makeprg, "./go.sh");
  TEST_CHECK(editor.make->config.mode == MakeMode_Exec);
  check_string(message(&editor), "build ok");
  TEST_CHECK(access(path_in_dir("session"), F_OK) != 0);  // used up
  TEST_CHECK(access(MAKE_LOG_PATH MAKE_RC_SUFFIX, F_OK) != 0);
  editor_deinit(&editor);

  // A failed build: the session comes back, then the first error is shown.
  make_session_add(&session, path_in_dir("b.c"), 0, 0, false);
  TEST_ASSERT(make_session_write(path_in_dir("session"), &session, NULL));
  make_session_clear(&session);
  write_file(MAKE_LOG_PATH, read_file(path_in_dir("log")));
  write_file(MAKE_LOG_PATH MAKE_RC_SUFFIX, "1\n");
  setup_editor(&editor);
  TEST_CHECK(editor_restore_session(&editor, path_in_dir("session")));
  check_string(buffer_get_filename(editor.current_buffer), path_in_dir("a.c"));
  TEST_CHECK(current_line(&editor) == 3);
  TEST_CHECK(strncmp(message(&editor), "build failed (exit 1), 2 errors", 31) == 0);
  editor_deinit(&editor);
  remove(MAKE_LOG_PATH);

  setup_editor(&editor);
  TEST_CHECK(!editor_restore_session(&editor, path_in_dir("missing")));
  editor_deinit(&editor);
  remove_temp_dir();
}

// Points stdin at /dev/null, so "press Enter" does not wait for a person.
static void stdin_from_dev_null(void) {
  int fd = open("/dev/null", O_RDONLY);
  TEST_ASSERT(fd >= 0);
  dup2(fd, STDIN_FILENO);
  close(fd);
}

void test_make_command_wait_mode(void) {
  make_temp_dir();
  stdin_from_dev_null();
  write_file(path_in_dir("a.c"), "int a;\nint b;\n");
  char script[512];
  snprintf(script, sizeof(script),
           "echo building $1\necho '%s:2: error: b'\nexit 4\n", path_in_dir("a.c"));
  write_file(path_in_dir("build"), script);

  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, path_in_dir("a.c"));
  type_text(&editor, "x");
  TEST_CHECK(buffer_is_modified(editor.current_buffer));
  char command[400];
  snprintf(command, sizeof(command), "set makeprg=/usr/bin/bash %s",
           path_in_dir("build"));
  enter_command(&editor, command);
  enter_command(&editor, "make fast");

  // Autowrite saved the edit before the build ran.
  TEST_CHECK(!buffer_is_modified(editor.current_buffer));
  check_string(read_file(path_in_dir("a.c")), "xint a;\nint b;\n");
  // tee kept the output; LOG.rc kept the build's status.
  const char* log = read_file(MAKE_LOG_PATH);
  TEST_CHECK(log != NULL && strstr(log, "building fast") != NULL);
  TEST_CHECK(strncmp(message(&editor), "build failed (exit 4), 1 error", 30) == 0);
  TEST_MSG("message: %s", message(&editor));
  TEST_CHECK(current_line(&editor) == 2);
  editor_deinit(&editor);
  remove(MAKE_LOG_PATH);
  remove_temp_dir();
}

void test_make_command_without_program(void) {
  make_temp_dir();
  char cwd[512];
  TEST_ASSERT(getcwd(cwd, sizeof(cwd)) != NULL);
  TEST_ASSERT(chdir(g_dir) == 0);  // no build.sh, no Makefile
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  enter_command(&editor, "make");
  TEST_CHECK(strstr(message(&editor), ":set makeprg=CMD") != NULL);
  TEST_MSG("message: %s", message(&editor));
  editor_deinit(&editor);
  TEST_ASSERT(chdir(cwd) == 0);
  remove_temp_dir();
}

void test_make_exec_mode_refuses_modified_unnamed_buffer(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  type_text(&editor, "unsaved");
  enter_command(&editor, "set makeprg=true");
  enter_command(&editor, "set makemode=exec");
  enter_command(&editor, "make");
  TEST_CHECK(strstr(message(&editor), "Unnamed buffer modified") != NULL);
  TEST_MSG("message: %s", message(&editor));
  TEST_CHECK(access(MAKE_SESSION_PATH, F_OK) != 0);  // nothing written
  editor_deinit(&editor);
}

TEST_LIST = {
  {"make_config_set", test_make_config_set},
  {"make_config_load_file_counts_bad_lines",
   test_make_config_load_file_counts_bad_lines},
  {"make_config_load_default_prefers_working_dir",
   test_make_config_load_default_prefers_working_dir},
  {"make_resolve_program", test_make_resolve_program},
  {"make_build_script", test_make_build_script},
  {"make_build_exec_script", test_make_build_exec_script},
  {"make_shell_quote", test_make_shell_quote},
  {"make_exec_script_runs_in_bash", test_make_exec_script_runs_in_bash},
  {"make_read_status", test_make_read_status},
  {"make_parse_error_lines", test_make_parse_error_lines},
  {"make_parse_log", test_make_parse_log},
  {"make_session_round_trip", test_make_session_round_trip},
  {"make_set_command", test_make_set_command},
  {"make_report_jumps_to_first_error", test_make_report_jumps_to_first_error},
  {"make_report_ok_and_no_errors", test_make_report_ok_and_no_errors},
  {"make_restore_session", test_make_restore_session},
  {"make_command_wait_mode", test_make_command_wait_mode},
  {"make_command_without_program", test_make_command_without_program},
  {"make_exec_mode_refuses_modified_unnamed_buffer",
   test_make_exec_mode_refuses_modified_unnamed_buffer},
  {NULL, NULL}};
