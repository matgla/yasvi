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

// The config file (config.c: ~/.config/vi.conf, :set) and the typing
// recorder (record.c: FILE.rec), alone and through the editor.

#define _GNU_SOURCE

#include "acutest.h"

#include "mock_ncurses.h"

#include "config.h"
#include "editor.h"
#include "make.h"
#include "record.h"
#include "termpanel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_dir[64];

static void make_temp_dir(void) {
  snprintf(g_dir, sizeof(g_dir), "/tmp/yasvi_record_XXXXXX");
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

// The lines of `text` that are not # comments, copied into `lines`.
static int data_lines(const char* text, char lines[][128], int max) {
  int count = 0;
  while (*text != '\0' && count < max) {
    const char* end = strchr(text, '\n');
    size_t length = end != NULL ? (size_t)(end - text) : strlen(text);
    if (text[0] != '#') {
      if (length > 127) {
        length = 127;
      }
      memcpy(lines[count], text, length);
      lines[count][length] = '\0';
      ++count;
    }
    if (end == NULL) {
      break;
    }
    text = end + 1;
  }
  return count;
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

/* ---- config.c ---------------------------------------------------------- */

void test_config_set_record(void) {
  EditorConfig config;
  config_init(&config);
  TEST_CHECK(!config.record);
  TEST_CHECK(config_set(&config, "record") == ConfigSet_Ok);
  TEST_CHECK(config.record);
  TEST_CHECK(config_set(&config, "norecord") == ConfigSet_Ok);
  TEST_CHECK(!config.record);
  TEST_CHECK(config_set(&config, "set record=on\n") == ConfigSet_Ok);
  TEST_CHECK(config.record);
  TEST_CHECK(config_set(&config, "  record = off  ") == ConfigSet_Ok);
  TEST_CHECK(!config.record);
  TEST_CHECK(config_set(&config, "record=yes") == ConfigSet_Ok);
  TEST_CHECK(config.record);
  TEST_CHECK(config_set(&config, "record=maybe") == ConfigSet_BadValue);
  TEST_CHECK(config.record);
  TEST_CHECK(config_set(&config, "norecord=on") == ConfigSet_BadValue);
  TEST_CHECK(config_set(&config, "record?") == ConfigSet_Query);
  TEST_CHECK(config_set(&config, "# record=off") == ConfigSet_Ignored);
  TEST_CHECK(config_set(&config, "") == ConfigSet_Ignored);
  TEST_CHECK(config_set(&config, "makeprg=make") == ConfigSet_UnknownKey);
  TEST_CHECK(config_set(&config, "recordings") == ConfigSet_UnknownKey);
  TEST_CHECK(config_set(&config, "record extra") == ConfigSet_UnknownKey);
  TEST_CHECK(config.record);
}

void test_config_describe(void) {
  EditorConfig config;
  config_init(&config);
  char out[64];
  config_describe(&config, out, sizeof(out));
  check_string(out, "norecord autoindent");
  config.record = true;
  config_describe(&config, out, sizeof(out));
  check_string(out, "record autoindent");
  // :set shows only the setting it names; ai is autoindent
  TEST_CHECK(config_set(&config, "noai") == ConfigSet_Ok);
  TEST_CHECK(!config.autoindent);
  config_describe_setting(&config, "noai", out, sizeof(out));
  check_string(out, "noautoindent");
  config_describe_setting(&config, "record?", out, sizeof(out));
  check_string(out, "record");
  TEST_CHECK(config_set(&config, "set autoindent=on") == ConfigSet_Ok);
  TEST_CHECK(config.autoindent);
}

void test_config_user_path(void) {
  char* path = config_user_path("/xdg", "/home/me");
  check_string(path, "/xdg/vi.conf");
  free(path);
  path = config_user_path("", "/home/me");
  check_string(path, "/home/me/.config/vi.conf");
  free(path);
  path = config_user_path(NULL, NULL);
  check_string(path, "/root/.config/vi.conf");
  free(path);
}

static bool count_known(void* context, const char* line) {
  ++*(int*)context;
  return strncmp(line, "good", 4) == 0;
}

void test_config_read_lines(void) {
  make_temp_dir();
  write_file(path_in_dir("vi.conf"), "good\nbad\ngood\n");
  int seen = 0;
  int bad = -1;
  TEST_CHECK(config_read_lines(path_in_dir("vi.conf"), count_known, &seen, &bad));
  TEST_CHECK(seen == 3);
  TEST_CHECK(bad == 1);
  TEST_CHECK(!config_read_lines(path_in_dir("missing"), count_known, &seen, &bad));
  remove_temp_dir();
}

/* ---- record.c ---------------------------------------------------------- */

void test_record_wanted_and_path(void) {
  TEST_CHECK(record_wanted("a.c"));
  TEST_CHECK(record_wanted("rec"));
  TEST_CHECK(!record_wanted("a.c.rec"));
  TEST_CHECK(!record_wanted(".rec"));
  TEST_CHECK(!record_wanted(""));
  TEST_CHECK(!record_wanted(NULL));
  char* path = record_path_for("dir/a.c");
  check_string(path, "dir/a.c.rec");
  free(path);
}

void test_record_key_names(void) {
  char name[16];
  record_key_name('a', NULL, name, sizeof(name));
  check_string(name, "a");
  record_key_name('#', NULL, name, sizeof(name));
  check_string(name, "#");
  record_key_name(' ', NULL, name, sizeof(name));
  check_string(name, "SPACE");
  record_key_name('\n', NULL, name, sizeof(name));
  check_string(name, "ENTER");
  record_key_name('\t', NULL, name, sizeof(name));
  check_string(name, "TAB");
  record_key_name(27, NULL, name, sizeof(name));
  check_string(name, "ESC");
  record_key_name(127, NULL, name, sizeof(name));
  check_string(name, "BS");
  record_key_name(2, NULL, name, sizeof(name));
  check_string(name, "^B");
  record_key_name(KEY_UP, "UP", name, sizeof(name));
  check_string(name, "UP");
  record_key_name(600, NULL, name, sizeof(name));
  check_string(name, "K600");
}

void test_record_format_time(void) {
  char out[32];
  record_format_time(time(NULL), out, sizeof(out));
  int y, mo, d, h, mi, s;
  TEST_CHECK(sscanf(out, "%4d-%2d-%2d %2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) == 6);
  TEST_MSG("got [%s]", out);
}

// Session, keys with their times and pauses, a pause line, and a second
// session appended after a switch to another file and back.
void test_recorder_writes_sessions_times_and_pauses(void) {
  make_temp_dir();
  const char* source = path_in_dir("a.c");
  const char* other = path_in_dir("b.c");
  Recorder recorder;
  recorder_init(&recorder);
  const time_t before = time(NULL);
  TEST_CHECK(recorder_key(&recorder, source, 3, 'i', "i", 1000));
  TEST_CHECK(recorder_key(&recorder, source, 3, 'x', "x", 1180));
  TEST_CHECK(recorder_key(&recorder, source, 3, 27, "ESC", 4500));  // 3.32 s pause
  recorder_idle(&recorder, 5000);  // under RECORD_FLUSH_MS since the last key
  TEST_CHECK(recorder.pending);
  recorder_idle(&recorder, 4500 + RECORD_FLUSH_MS);
  TEST_CHECK(!recorder.pending);
  TEST_CHECK(recorder_key(&recorder, other, 1, 'j', "j", 6000));
  TEST_CHECK(recorder_key(&recorder, source, 4, 'k', "k", 7000));
  const time_t after = time(NULL);
  recorder_close(&recorder);

  const char* text = read_file(path_in_dir("a.c.rec"));
  TEST_ASSERT(text != NULL);
  TEST_CHECK(strncmp(text, "# yasvi recording of a.c\n", 25) == 0);
  TEST_MSG("got [%s]", text);
  char lines[16][128];
  const int count = data_lines(text, lines, 16);
  TEST_CHECK(count == 7);
  TEST_MSG("got %d lines in [%s]", count, text);
  if (count != 7) {
    remove_temp_dir();
    return;
  }

  char date[16], clock[16];
  long long unix_time = 0;
  int lines_in_file = -1;
  TEST_CHECK(sscanf(lines[0], "session %15s %15s unix %lld lines %d", date, clock,
                    &unix_time, &lines_in_file) == 4);
  TEST_MSG("got [%s]", lines[0]);
  TEST_CHECK(unix_time >= before && unix_time <= after);
  TEST_CHECK(lines_in_file == 3);
  char expected_date[32];
  record_format_time((time_t)unix_time, expected_date, sizeof(expected_date));
  char got_date[40];
  snprintf(got_date, sizeof(got_date), "%s %s", date, clock);
  check_string(got_date, expected_date);

  check_string(lines[1], "0 0 105 i");
  check_string(lines[2], "180 180 120 x");
  TEST_CHECK(strncmp(lines[3], "pause ", 6) == 0);
  TEST_CHECK(strstr(lines[3], " after 3.320 s") != NULL);
  TEST_MSG("got [%s]", lines[3]);
  check_string(lines[4], "3500 3320 27 ESC");
  TEST_CHECK(strncmp(lines[5], "session ", 8) == 0);
  TEST_CHECK(strstr(lines[5], " lines 4") != NULL);
  check_string(lines[6], "0 0 107 k");

  text = read_file(path_in_dir("b.c.rec"));
  TEST_ASSERT(text != NULL);
  TEST_CHECK(data_lines(text, lines, 16) == 2);
  check_string(lines[1], "0 0 106 j");
  remove_temp_dir();
}

void test_recorder_reports_unwritable_once(void) {
  Recorder recorder;
  recorder_init(&recorder);
  TEST_CHECK(!recorder_key(&recorder, "/nonexistent-dir/a.c", 1, 'a', "a", 0));
  TEST_CHECK(recorder_key(&recorder, "/nonexistent-dir/a.c", 1, 'b', "b", 10));
  recorder_close(&recorder);
}

/* ---- through the editor ------------------------------------------------ */

// vi.conf turns recording on; keys typed in a file land in FILE.rec, the
// :set line itself included; :set norecord stops it.
void test_editor_records_keys_from_config(void) {
  make_temp_dir();
  char* old_xdg = getenv("XDG_CONFIG_HOME") ? strdup(getenv("XDG_CONFIG_HOME")) : NULL;
  setenv("XDG_CONFIG_HOME", g_dir, 1);
  char* old_home = getenv("HOME") ? strdup(getenv("HOME")) : NULL;
  setenv("HOME", g_dir, 1);  // no ~/.yasvirc of the user's
  write_file(path_in_dir("vi.conf"), "# mine\nset record\n");
  write_file(path_in_dir("a.c"), "one\ntwo\n");

  Editor editor;
  setup_editor(&editor);
  editor_load_config(&editor);
  TEST_CHECK(editor.config.record);
  TEST_CHECK(editor.error_message == NULL);
  editor_load_file(&editor, path_in_dir("a.c"));
  editor_process_key(&editor, 'j');
  editor_process_key(&editor, KEY_DOWN);
  enter_command(&editor, "set norecord");
  check_string(message(&editor), "norecord");
  TEST_CHECK(editor.recorder == NULL);
  editor_process_key(&editor, 'k');  // not recorded
  editor_deinit(&editor);

  const char* text = read_file(path_in_dir("a.c.rec"));
  TEST_ASSERT(text != NULL);
  char lines[32][128];
  const int count = data_lines(text, lines, 32);
  // session, j, DOWN, ':', the 12 characters of "set norecord", ENTER
  TEST_CHECK(count == 1 + 2 + 1 + 12 + 1);
  TEST_MSG("got %d lines in [%s]", count, text);
  TEST_CHECK(strstr(lines[0], " lines 2") != NULL);
  TEST_CHECK(strstr(lines[1], " 106 j") != NULL);
  TEST_CHECK(strstr(lines[2], " DOWN") != NULL);
  TEST_CHECK(strstr(lines[3], " 58 :") != NULL);
  TEST_CHECK(strstr(lines[count - 1], " 10 ENTER") != NULL);
  TEST_CHECK(strstr(text, " k\n") == NULL);

  if (old_xdg != NULL) {
    setenv("XDG_CONFIG_HOME", old_xdg, 1);
    free(old_xdg);
  } else {
    unsetenv("XDG_CONFIG_HOME");
  }
  if (old_home != NULL) {
    setenv("HOME", old_home, 1);
    free(old_home);
  }
  remove_temp_dir();
}

void test_editor_set_record_and_bad_value(void) {
  make_temp_dir();
  write_file(path_in_dir("a.c"), "x\n");
  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, path_in_dir("a.c"));
  editor_process_key(&editor, 'l');  // recording is off by default
  enter_command(&editor, "set record=sometimes");
  TEST_CHECK(strstr(message(&editor), "record is on or off") != NULL);
  TEST_CHECK(!editor.config.record);
  enter_command(&editor, "set record");
  check_string(message(&editor), "record");
  TEST_CHECK(editor.config.record);
  TEST_CHECK(access(path_in_dir("a.c.rec"), F_OK) != 0);  // nothing typed since
  editor_process_key(&editor, 'h');
  editor_deinit(&editor);
  const char* text = read_file(path_in_dir("a.c.rec"));
  TEST_ASSERT(text != NULL);
  char lines[8][128];
  TEST_CHECK(data_lines(text, lines, 8) == 2);
  check_string(lines[1], "0 0 104 h");
  remove_temp_dir();
}

// A config file line neither config.c nor make.c knows is reported.
void test_editor_config_bad_line_reported(void) {
  make_temp_dir();
  char* old_xdg = getenv("XDG_CONFIG_HOME") ? strdup(getenv("XDG_CONFIG_HOME")) : NULL;
  setenv("XDG_CONFIG_HOME", g_dir, 1);
  char* old_home = getenv("HOME") ? strdup(getenv("HOME")) : NULL;
  setenv("HOME", g_dir, 1);  // no ~/.yasvirc of the user's
  write_file(path_in_dir("vi.conf"), "record=on\nmakemode=exec\nnumber\n");
  Editor editor;
  setup_editor(&editor);
  editor_load_config(&editor);
  TEST_CHECK(editor.config.record);
  TEST_CHECK(editor.make != NULL && editor.make->config.mode == MakeMode_Exec);
  TEST_CHECK(strstr(message(&editor), "vi.conf:3: unknown \"number\" (record, ai,") !=
             NULL);
  TEST_MSG("got [%s]", message(&editor));
  editor_deinit(&editor);
  if (old_xdg != NULL) {
    setenv("XDG_CONFIG_HOME", old_xdg, 1);
    free(old_xdg);
  } else {
    unsetenv("XDG_CONFIG_HOME");
  }
  if (old_home != NULL) {
    setenv("HOME", old_home, 1);
    free(old_home);
  }
  remove_temp_dir();
}

// The :term panel's session goes to the recording too: focus changes, the
// lines sent to the shell, what it printed and its exit.
static bool wait_for_panel_line(Editor* editor, const char* text) {
  for (int tries = 0; tries < 500; ++tries) {
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

void test_editor_records_terminal(void) {
  make_temp_dir();
  write_file(path_in_dir("a.c"), "x\n");
  Editor editor;
  setup_editor(&editor);
  editor.config.record = true;
  editor_load_file(&editor, path_in_dir("a.c"));
  enter_command(&editor, "term echo from-term");
  TEST_ASSERT(editor.terminal != NULL);
  TEST_CHECK(wait_for_panel_line(&editor, "from-term"));
  const char* typed = "exit 3";
  for (const char* p = typed; *p != '\0'; ++p) {
    editor_process_key(&editor, (unsigned char)*p);
  }
  editor_process_key(&editor, '\n');
  TEST_CHECK(wait_for_panel_line(&editor, "[shell exited 3]"));
  editor_process_key(&editor, 0x0b);  // Ctrl-K: back to the text
  editor_deinit(&editor);

  const char* text = read_file(path_in_dir("a.c.rec"));
  TEST_ASSERT(text != NULL);
  TEST_CHECK(strstr(text, "# term AT_MS $ LINE") != NULL);  // the header says so
  char lines[64][128];
  const int count = data_lines(text, lines, 64);
  // the words of the lines that are not keys, in order
  char words[512] = "";
  for (int i = 0; i < count; ++i) {
    char word[16];
    long long at;
    char rest[112] = "";
    if (sscanf(lines[i], "%15s %lld %111[^\n]", word, &at, rest) >= 2 &&
        strcmp(word, "session") != 0 && (word[0] < '0' || word[0] > '9')) {
      strcat(words, word);
      strcat(words, rest[0] != '\0' ? " " : "");
      strcat(words, rest);
      strcat(words, "|");
    }
  }
  check_string(words,
               "term $ echo from-term|term focus|out from-term|"
               "term $ exit 3|term exit 3|text focus|");
  remove_temp_dir();
}

TEST_LIST = {
  {"config_set_record", test_config_set_record},
  {"config_describe", test_config_describe},
  {"config_user_path", test_config_user_path},
  {"config_read_lines", test_config_read_lines},
  {"record_wanted_and_path", test_record_wanted_and_path},
  {"record_key_names", test_record_key_names},
  {"record_format_time", test_record_format_time},
  {"recorder_writes_sessions_times_and_pauses",
   test_recorder_writes_sessions_times_and_pauses},
  {"recorder_reports_unwritable_once", test_recorder_reports_unwritable_once},
  {"editor_records_keys_from_config", test_editor_records_keys_from_config},
  {"editor_set_record_and_bad_value", test_editor_set_record_and_bad_value},
  {"editor_config_bad_line_reported", test_editor_config_bad_line_reported},
  {"editor_records_terminal", test_editor_records_terminal},
  {NULL, NULL}};
