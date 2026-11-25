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

// The :term panel: termpanel.c's output cleaning, scrollback and history, a
// real shell (the host's bash) on pipes, and the panel through the editor.

#define _GNU_SOURCE

#include "acutest.h"

#include "mock_ncurses.h"

#include "editor.h"
#include "termpanel.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static void check_string(const char* actual, const char* expected) {
  TEST_CHECK(actual != NULL && strcmp(actual, expected) == 0);
  TEST_MSG("got      [%s]", actual != NULL ? actual : "(null)");
  TEST_MSG("expected [%s]", expected);
}

static void feed(TermPanel* panel, const char* text) {
  termpanel_feed(panel, text, strlen(text));
}

static const char* last_line(const TermPanel* panel) {
  return termpanel_line(panel, termpanel_line_count(panel) - 1);
}

static long long now_ms(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

// Polls until a scrollback line equals `text` (true) or 5 s pass.
static bool wait_for_line(TermPanel* panel, const char* text) {
  const long long end = now_ms() + 5000;
  while (now_ms() < end) {
    termpanel_poll(panel);
    for (int i = 0; i < termpanel_line_count(panel); ++i) {
      if (strcmp(termpanel_line(panel, i), text) == 0) {
        return true;
      }
    }
    usleep(10000);
  }
  return false;
}

static void dump(const TermPanel* panel) {
  for (int i = 0; i < termpanel_line_count(panel); ++i) {
    TEST_MSG("line %d: [%s]", i, termpanel_line(panel, i));
  }
}

/* ---- output cleaning --------------------------------------------------- */

void test_feed_lines_and_controls(void) {
  TermPanel panel;
  termpanel_init(&panel);
  feed(&panel, "one\ntwo\r\nthr");
  TEST_CHECK(termpanel_line_count(&panel) == 2);
  check_string(termpanel_line(&panel, 0), "one");
  check_string(termpanel_line(&panel, 1), "two");
  TEST_CHECK(panel.partial_length == 3);  // "thr" still arriving
  feed(&panel, "ee\n");
  check_string(last_line(&panel), "three");
  feed(&panel, "\x1b[1;31m" "red" "\x1b[0m plain\n");
  check_string(last_line(&panel), "red plain");
  feed(&panel, "\x1b]0;title\x07" "after osc\n");
  check_string(last_line(&panel), "after osc");
  feed(&panel, " 10%\r 50%\r100%\n");
  check_string(last_line(&panel), "100%");
  feed(&panel, "abc\b\bX\n");
  check_string(last_line(&panel), "aX");
  feed(&panel, "a\tb\n");
  check_string(last_line(&panel), "a       b");
  feed(&panel, "bell\x07\x01" "!\n");
  check_string(last_line(&panel), "bell!");
  termpanel_deinit(&panel);
}

void test_feed_long_line_and_ring(void) {
  TermPanel panel;
  termpanel_init(&panel);
  char* long_line = malloc(TERMPANEL_LINE_MAX + 11);
  memset(long_line, 'x', TERMPANEL_LINE_MAX + 10);
  long_line[TERMPANEL_LINE_MAX + 10] = '\n';
  termpanel_feed(&panel, long_line, TERMPANEL_LINE_MAX + 11);
  free(long_line);
  TEST_CHECK(termpanel_line_count(&panel) == 2);
  TEST_CHECK(strlen(termpanel_line(&panel, 0)) == TERMPANEL_LINE_MAX);
  TEST_CHECK(strlen(termpanel_line(&panel, 1)) == 10);

  char line[32];
  for (int i = 0; i < TERMPANEL_SCROLLBACK + 5; ++i) {
    snprintf(line, sizeof(line), "line %d\n", i);
    feed(&panel, line);
  }
  TEST_CHECK(termpanel_line_count(&panel) == TERMPANEL_SCROLLBACK);
  snprintf(line, sizeof(line), "line %d", TERMPANEL_SCROLLBACK + 4);
  check_string(last_line(&panel), line);
  check_string(termpanel_line(&panel, 0), "line 5");  // the oldest kept (2 long lines came first)
  TEST_CHECK(termpanel_line(&panel, TERMPANEL_SCROLLBACK) == NULL);
  termpanel_deinit(&panel);
}

void test_input_and_history(void) {
  TermPanel panel;
  termpanel_init(&panel);
  termpanel_input_char(&panel, 'l');
  termpanel_input_char(&panel, 's');
  termpanel_input_char(&panel, 'x');
  termpanel_input_backspace(&panel);
  check_string(panel.input, "ls");
  // Sending needs a shell; without one the line still clears.
  TEST_CHECK(!termpanel_submit(&panel));
  check_string(panel.input, "");
  termpanel_deinit(&panel);
}

/* ---- a real shell ------------------------------------------------------ */

void test_shell_runs_commands_and_keeps_state(void) {
  TermPanel panel;
  termpanel_init(&panel);
  TEST_ASSERT(termpanel_start(&panel));
  TEST_CHECK(termpanel_running(&panel));
  TEST_CHECK(termpanel_send_line(&panel, "echo hello; echo oops >&2"));
  check_string(termpanel_line(&panel, 0), "$ echo hello; echo oops >&2");
  TEST_CHECK(wait_for_line(&panel, "hello"));
  TEST_CHECK(wait_for_line(&panel, "oops"));
  // One shell: cd and variables last.
  termpanel_send_line(&panel, "cd /tmp; X=kept");
  termpanel_send_line(&panel, "echo \"$PWD $X $TERM\"");
  TEST_CHECK(wait_for_line(&panel, "/tmp kept dumb"));
  dump(&panel);
  // A program reading stdin reads the next line typed.
  termpanel_send_line(&panel, "read answer; echo got $answer");
  termpanel_send_line(&panel, "42");
  TEST_CHECK(wait_for_line(&panel, "got 42"));

  // History: Up goes back through the lines sent.
  termpanel_history_step(&panel, -1);
  check_string(panel.input, "42");
  termpanel_history_step(&panel, -1);
  check_string(panel.input, "read answer; echo got $answer");
  termpanel_history_step(&panel, 1);
  termpanel_history_step(&panel, 1);
  check_string(panel.input, "");

  termpanel_send_line(&panel, "exit 7");
  TEST_CHECK(wait_for_line(&panel, "[shell exited 7]"));
  TEST_CHECK(!termpanel_running(&panel));
  TEST_CHECK(panel.exit_status == 7);
  TEST_CHECK(!termpanel_send_line(&panel, "echo nobody"));
  termpanel_deinit(&panel);
}

void test_interrupt_stops_command_not_shell(void) {
  TermPanel panel;
  termpanel_init(&panel);
  TEST_ASSERT(termpanel_start(&panel));
  termpanel_send_line(&panel, "sleep 30; echo slept");
  // Wait until the sleep is running under the shell.
  pid_t below[8];
  int count = 0;
  const long long end = now_ms() + 5000;
  while (now_ms() < end && (count = termpanel_descendants(panel.pid, below, 8)) == 0) {
    usleep(10000);
  }
  TEST_ASSERT(count > 0);
  const long long started = now_ms();
  TEST_CHECK(termpanel_interrupt(&panel) > 0);
  check_string(last_line(&panel), "^C");
  // bash goes on to the next command after its child dies of SIGINT.
  termpanel_send_line(&panel, "echo still here");
  TEST_CHECK(wait_for_line(&panel, "still here"));
  TEST_CHECK(now_ms() - started < 10000);
  TEST_CHECK(termpanel_running(&panel));
  termpanel_stop(&panel);
  TEST_CHECK(!termpanel_running(&panel));
  TEST_CHECK(panel.to_shell < 0 && panel.from_shell < 0);
  termpanel_deinit(&panel);
}

/* ---- through the editor ------------------------------------------------ */

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

static void type(Editor* editor, const char* text) {
  for (const char* p = text; *p != '\0'; ++p) {
    editor_process_key(editor, *p);
  }
}

static void enter_command(Editor* editor, const char* cmd) {
  editor_process_key(editor, ':');
  type(editor, cmd);
  editor_process_key(editor, '\n');
}

// Polls through editor_idle until a scrollback line equals `text`.
static bool editor_wait_for_line(Editor* editor, const char* text) {
  const long long end = now_ms() + 5000;
  while (now_ms() < end) {
    editor_idle(editor);
    TermPanel* panel = editor->terminal;
    for (int i = 0; panel != NULL && i < termpanel_line_count(panel); ++i) {
      if (strcmp(termpanel_line(panel, i), text) == 0) {
        return true;
      }
    }
    usleep(10000);
  }
  return false;
}

void test_editor_term_panel_keys(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  for (int i = 0; i < 40; ++i) {
    buffer_append_line(editor.current_buffer, "text");
  }
  editor.cursor.y = 20;  // near the bottom: the panel will push it up

  enter_command(&editor, "term echo from-term");
  TermPanel* panel = editor.terminal;
  TEST_ASSERT(panel != NULL);
  TEST_CHECK(panel->visible && panel->focused);
  TEST_CHECK(panel->rows == 8);  // a third of 24
  TEST_CHECK(editor.cursor.y < 24 - 3 - panel->rows);
  TEST_CHECK(editor_wait_for_line(&editor, "from-term"));

  // Typing goes to the panel, Enter sends it.
  type(&editor, "echo typed");
  check_string(panel->input, "echo typed");
  editor_process_key(&editor, '\n');
  TEST_CHECK(editor_wait_for_line(&editor, "typed"));
  editor_redraw_screen(&editor);  // draws without crashing

  // Ctrl-K back to the text; keys move the text cursor again.
  editor_process_key(&editor, 0x0b);
  TEST_CHECK(!panel->focused);
  type(&editor, "j");
  check_string(panel->input, "");
  // Ctrl-J (Enter) in normal mode goes down to the panel.
  editor_process_key(&editor, '\n');
  TEST_CHECK(panel->focused);
  // Esc goes up; Ctrl-W j down; Ctrl-W w toggles.
  editor_process_key(&editor, 27);
  TEST_CHECK(!panel->focused);
  editor_process_key(&editor, 0x17);
  editor_process_key(&editor, 'j');
  TEST_CHECK(panel->focused);
  editor_process_key(&editor, 0x17);
  editor_process_key(&editor, 'w');
  TEST_CHECK(!panel->focused);
  // Ctrl-W c closes the panel and its shell.
  editor_process_key(&editor, 0x17);
  editor_process_key(&editor, 'c');
  TEST_CHECK(editor.terminal == NULL);
  editor_redraw_screen(&editor);

  // :term again starts a new shell; :termclose closes it.
  enter_command(&editor, "term");
  TEST_CHECK(editor.terminal != NULL && termpanel_running(editor.terminal));
  editor_process_key(&editor, 27);
  enter_command(&editor, "termclose");
  TEST_CHECK(editor.terminal == NULL);
  editor_deinit(&editor);
}

/* ---- Tab completion ------------------------------------------------------ */

static void touch_file(const char* dir, const char* name) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", dir, name);
  const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
  TEST_ASSERT(fd >= 0);
  close(fd);
}

static void make_dir(const char* dir, const char* name) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", dir, name);
  TEST_ASSERT(mkdir(path, 0755) == 0);
}

static void set_input(TermPanel* panel, const char* text) {
  termpanel_input_clear(panel);
  for (const char* c = text; *c != '\0'; ++c) {
    termpanel_input_char(panel, *c);
  }
}

static void remove_tree(const char* dir) {
  char cmd[600];
  snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
  TEST_CHECK(system(cmd) == 0);
}

// File names come from the shell's directory (after its cd), not vi's.
void test_complete_files_in_shell_directory(void) {
  char dir[] = "/tmp/yasvi_complete_XXXXXX";
  TEST_ASSERT(mkdtemp(dir) != NULL);
  touch_file(dir, "alpha.txt");
  make_dir(dir, "alpine");
  touch_file(dir, "alpine/inner.c");
  touch_file(dir, "beta file.c");
  touch_file(dir, ".hidden");

  TermPanel panel;
  termpanel_init(&panel);
  TEST_ASSERT(termpanel_start(&panel));
  char cd[600];
  snprintf(cd, sizeof(cd), "cd '%s' && echo ready", dir);
  TEST_ASSERT(termpanel_send_line(&panel, cd));
  TEST_ASSERT(wait_for_line(&panel, "ready"));
  char* cwd = termpanel_shell_cwd(&panel);
  check_string(cwd, dir);
  free(cwd);

  set_input(&panel, "cat al");
  TEST_CHECK(termpanel_complete(&panel, 80) == 2);
  check_string(panel.input, "cat alp");  // what both start with
  const int lines = termpanel_line_count(&panel);
  TEST_CHECK(termpanel_complete(&panel, 80) == 2);  // nothing to add: listed
  check_string(panel.input, "cat alp");
  TEST_CHECK(termpanel_line_count(&panel) == lines + 1);
  check_string(last_line(&panel), "alpha.txt  alpine/");
  termpanel_input_char(&panel, 'h');
  TEST_CHECK(termpanel_complete(&panel, 80) == 1);
  check_string(panel.input, "cat alpha.txt ");

  set_input(&panel, "ls alpi");
  termpanel_complete(&panel, 80);
  check_string(panel.input, "ls alpine/");  // a directory: no space
  termpanel_complete(&panel, 80);
  check_string(panel.input, "ls alpine/inner.c ");

  set_input(&panel, "wc -l be");
  termpanel_complete(&panel, 80);
  check_string(panel.input, "wc -l beta\\ file.c ");
  set_input(&panel, "wc \"beta f");
  termpanel_complete(&panel, 80);
  check_string(panel.input, "wc beta\\ file.c ");
  set_input(&panel, "echo x > .h");
  termpanel_complete(&panel, 80);
  check_string(panel.input, "echo x > .hidden ");
  set_input(&panel, "cat nothing");
  TEST_CHECK(termpanel_complete(&panel, 80) == 0);
  check_string(panel.input, "cat nothing");

  // the listing packs names into lines of the given width
  set_input(&panel, "ls ");
  TEST_CHECK(termpanel_complete(&panel, 20) == 3);
  check_string(termpanel_line(&panel, termpanel_line_count(&panel) - 2), "alpha.txt  alpine/");
  check_string(last_line(&panel), "beta file.c");

  termpanel_deinit(&panel);
  remove_tree(dir);
}

// The first word of a command: shell builtins and the programs in PATH.
void test_complete_commands(void) {
  char dir[] = "/tmp/yasvi_complete_bin_XXXXXX";
  TEST_ASSERT(mkdtemp(dir) != NULL);
  touch_file(dir, "yvtool-one");
  touch_file(dir, "yvtool-two");
  make_dir(dir, "yvtool-dir");  // not a program
  const char* old_path = getenv("PATH");
  char* saved = strdup(old_path != NULL ? old_path : "/bin:/usr/bin");
  char path[600];
  snprintf(path, sizeof(path), "%s:%s", dir, saved);
  setenv("PATH", path, 1);

  TermPanel panel;
  termpanel_init(&panel);
  set_input(&panel, "yvto");
  TEST_CHECK(termpanel_complete(&panel, 80) == 2);
  check_string(panel.input, "yvtool-");
  termpanel_input_char(&panel, 'o');
  TEST_CHECK(termpanel_complete(&panel, 80) == 1);
  check_string(panel.input, "yvtool-one ");
  set_input(&panel, "echo x | yvtool-t");
  termpanel_complete(&panel, 80);
  check_string(panel.input, "echo x | yvtool-two ");
  set_input(&panel, "true && yvtool-o");
  termpanel_complete(&panel, 80);
  check_string(panel.input, "true && yvtool-one ");
  set_input(&panel, "unal");
  TEST_CHECK(termpanel_complete(&panel, 80) == 1);
  check_string(panel.input, "unalias ");
  set_input(&panel, "echo yvtool");  // an argument: a file name, none here
  TEST_CHECK(termpanel_complete(&panel, 80) == 0);
  check_string(panel.input, "echo yvtool");
  set_input(&panel, "");
  TEST_CHECK(termpanel_complete(&panel, 80) == 0);  // not every program

  termpanel_deinit(&panel);
  setenv("PATH", saved, 1);
  free(saved);
  remove_tree(dir);
}

// Tab in the panel completes instead of typing a tab.
void test_editor_term_tab_completes(void) {
  char dir[] = "/tmp/yasvi_complete_ed_XXXXXX";
  TEST_ASSERT(mkdtemp(dir) != NULL);
  touch_file(dir, "unique_name.txt");
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  char cmd[600];
  snprintf(cmd, sizeof(cmd), "term cd '%s' && echo there", dir);
  enter_command(&editor, cmd);
  TEST_ASSERT(editor.terminal != NULL && editor.terminal->focused);
  TEST_ASSERT(editor_wait_for_line(&editor, "there"));
  type(&editor, "cat uni");
  editor_process_key(&editor, '\t');
  check_string(editor.terminal->input, "cat unique_name.txt ");
  editor_deinit(&editor);
  remove_tree(dir);
}

// A bare :term with the panel open closes it (toggle); :term CMD keeps it.
void test_editor_term_toggles_panel(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  enter_command(&editor, "term");
  TEST_ASSERT(editor.terminal != NULL && termpanel_running(editor.terminal));
  TermPanel* panel = editor.terminal;
  const int pid = panel->pid;
  editor_process_key(&editor, 27);  // back to the text
  enter_command(&editor, "term echo still");
  TEST_CHECK(editor.terminal == panel && panel->focused && panel->pid == pid);
  TEST_CHECK(editor_wait_for_line(&editor, "still"));
  editor_process_key(&editor, 27);
  enter_command(&editor, "term");
  TEST_CHECK(editor.terminal == NULL);
  enter_command(&editor, "term");  // and opens it again
  TEST_CHECK(editor.terminal != NULL && termpanel_running(editor.terminal));
  editor_deinit(&editor);
}

void test_editor_insert_mode_keeps_enter(void) {
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  enter_command(&editor, "term");
  editor_process_key(&editor, 27);  // back to the text
  TEST_ASSERT(editor.terminal != NULL && !editor.terminal->focused);
  type(&editor, "iab");
  editor_process_key(&editor, '\n');  // breaks the line, does not move windows
  TEST_CHECK(!editor.terminal->focused);
  TEST_CHECK(buffer_get_number_of_lines(editor.current_buffer) == 2);
  editor_process_key(&editor, 27);
  editor_deinit(&editor);  // kills the shell
}

TEST_LIST = {
  {"feed_lines_and_controls", test_feed_lines_and_controls},
  {"feed_long_line_and_ring", test_feed_long_line_and_ring},
  {"input_and_history", test_input_and_history},
  {"shell_runs_commands_and_keeps_state", test_shell_runs_commands_and_keeps_state},
  {"interrupt_stops_command_not_shell", test_interrupt_stops_command_not_shell},
  {"editor_term_panel_keys", test_editor_term_panel_keys},
  {"editor_term_toggles_panel", test_editor_term_toggles_panel},
  {"editor_insert_mode_keeps_enter", test_editor_insert_mode_keeps_enter},
  {"complete_files_in_shell_directory", test_complete_files_in_shell_directory},
  {"complete_commands", test_complete_commands},
  {"editor_term_tab_completes", test_editor_term_tab_completes},
  {NULL, NULL}};
