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
#include "man.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- helpers ---------------------------------------------------------- */

static char g_dir[64];

static void make_temp_dir(void) {
  snprintf(g_dir, sizeof(g_dir), "/tmp/yasvi_man_XXXXXX");
  TEST_ASSERT(mkdtemp(g_dir) != NULL);
}

static void remove_temp_dir(void) {
  char cmd[160];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
  TEST_CHECK(system(cmd) == 0);
  unsetenv("MANPATH");
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

// Two MANPATH directories, d1 and d2, and "DIR1:DIR2" in `manpath`.
static void make_man_dirs(char* manpath, size_t size) {
  TEST_ASSERT(mkdir(path_in_dir("d1"), 0755) == 0);
  TEST_ASSERT(mkdir(path_in_dir("d2"), 0755) == 0);
  snprintf(manpath, size, "%s/d1:%s/d2", g_dir, g_dir);
}

// Writes DIR/NAME as gzip of `text` (gzip from the host).
static void write_gzipped(const char* path, const char* text) {
  char plain[320];
  snprintf(plain, sizeof(plain), "%s.plain", path);
  write_file(plain, text);
  char cmd[700];
  snprintf(cmd, sizeof(cmd), "gzip -c %s > %s && rm %s", plain, path, plain);
  TEST_ASSERT(system(cmd) == 0);
}

static void check_path(char* found, const char* expected) {
  TEST_CHECK(found != NULL);
  if (found != NULL) {
    TEST_CHECK(strcmp(found, expected) == 0);
    TEST_MSG("found [%s], expected [%s]", found, expected);
  }
  free(found);
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

static const char* current_name(const Editor* editor) {
  return buffer_get_filename(editor->current_buffer);
}

static const char* first_row(const Editor* editor) {
  const BufferRow* row = buffer_get_first_row(editor->current_buffer);
  return row != NULL && row->data != NULL ? row->data : "";
}

/* ---- lookup ----------------------------------------------------------- */

void test_man_find_plain_page(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d1/printf.txt"), "PRINTF\n");
  check_path(man_find_page(manpath, "printf"), path_in_dir("d1/printf.txt"));
  TEST_CHECK(man_find_page(manpath, "nosuch") == NULL);
  TEST_CHECK(man_find_page(manpath, "") == NULL);
  TEST_CHECK(man_find_page(manpath, NULL) == NULL);
  remove_temp_dir();
}

void test_man_find_gzipped_page(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d2/tcc.txt.gz"), "not really gzip");
  check_path(man_find_page(manpath, "tcc"), path_in_dir("d2/tcc.txt.gz"));
  remove_temp_dir();
}

void test_man_plain_before_gzipped_in_one_dir(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d1/pio.txt.gz"), "z");
  write_file(path_in_dir("d1/pio.txt"), "plain");
  check_path(man_find_page(manpath, "pio"), path_in_dir("d1/pio.txt"));
  remove_temp_dir();
}

void test_man_directory_order_wins_over_compression(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  // in_dir is asked per directory, so d1's .gz beats d2's plain page.
  write_file(path_in_dir("d1/f_open.txt.gz"), "z");
  write_file(path_in_dir("d2/f_open.txt"), "plain");
  check_path(man_find_page(manpath, "f_open"), path_in_dir("d1/f_open.txt.gz"));
  remove_temp_dir();
}

void test_man_aliases(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d2/aliases"),
             "fprintf printf\nsys/stat.h sys-stat.h\nsnprintf printf\n");
  write_file(path_in_dir("d2/printf.txt.gz"), "z");
  write_file(path_in_dir("d2/sys-stat.h.txt"), "SYS/STAT.H\n");
  check_path(man_find_page(manpath, "fprintf"), path_in_dir("d2/printf.txt.gz"));
  check_path(man_find_page(manpath, "snprintf"), path_in_dir("d2/printf.txt.gz"));
  check_path(man_find_page(manpath, "sys/stat.h"), path_in_dir("d2/sys-stat.h.txt"));
  // A prefix of an alias name is not that name.
  TEST_CHECK(man_find_page(manpath, "fprint") == NULL);
  remove_temp_dir();
}

void test_man_pages_before_aliases(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  // Every directory is searched for NAME.txt before any aliases file.
  write_file(path_in_dir("d1/aliases"), "puts printf\n");
  write_file(path_in_dir("d1/printf.txt"), "PRINTF\n");
  write_file(path_in_dir("d2/puts.txt"), "PUTS\n");
  check_path(man_find_page(manpath, "puts"), path_in_dir("d2/puts.txt"));
  remove_temp_dir();
}

void test_man_alias_to_missing_page_tries_next_dir(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d1/aliases"), "fputs gone\n");
  write_file(path_in_dir("d2/aliases"), "fputs puts\n");
  write_file(path_in_dir("d2/puts.txt"), "PUTS\n");
  check_path(man_find_page(manpath, "fputs"), path_in_dir("d2/puts.txt"));
  remove_temp_dir();
}

void test_man_empty_entries_and_default(void) {
  make_temp_dir();
  char manpath[300];
  TEST_ASSERT(mkdir(path_in_dir("d1"), 0755) == 0);
  write_file(path_in_dir("d1/ls.txt"), "LS\n");
  snprintf(manpath, sizeof(manpath), "::%s/nowhere::%s/d1:", g_dir, g_dir);
  check_path(man_find_page(manpath, "ls"), path_in_dir("d1/ls.txt"));
  // NULL and "" mean MAN_DEFAULT_PATH; nothing by this name lives there.
  TEST_CHECK(man_find_page(NULL, "yasvi-no-such-page-xyz") == NULL);
  TEST_CHECK(man_find_page("", "yasvi-no-such-page-xyz") == NULL);
  remove_temp_dir();
}

void test_man_whatis(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  TEST_CHECK(man_find_whatis(manpath) == NULL);
  write_file(path_in_dir("d2/whatis"), "printf - formatted output\n");
  check_path(man_find_whatis(manpath), path_in_dir("d2/whatis"));
  remove_temp_dir();
}

void test_man_temp_path_and_gzip_check(void) {
  check_path(man_temp_path("/tmp", "/usr/share/man/printf.txt.gz"),
             "/tmp/yasvi-man-printf.txt");
  check_path(man_temp_path("/t", "sys-stat.h.txt.gz"),
             "/t/yasvi-man-sys-stat.h.txt");
  TEST_CHECK(man_is_gzipped("/a/b.txt.gz"));
  TEST_CHECK(!man_is_gzipped("/a/b.txt"));
  TEST_CHECK(!man_is_gzipped("gz"));
}

/* ---- :man and K ------------------------------------------------------- */

void test_man_command_opens_and_reuses_page(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d1/printf.txt"), "PRINTF(3)\nformatted output\n");
  setenv("MANPATH", manpath, 1);
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  enter_command(&editor, "man printf");
  TEST_CHECK(editor.number_of_buffers == 2);
  TEST_CHECK(strcmp(current_name(&editor), path_in_dir("d1/printf.txt")) == 0);
  TEST_CHECK(editor.current_buffer->autosave_disabled);
  TEST_CHECK(strcmp(first_row(&editor), "PRINTF(3)") == 0);

  editor_switch_to_buffer_by_index(&editor, 0);
  enter_command(&editor, "man printf");
  TEST_CHECK(editor.number_of_buffers == 2);  // reused, not loaded again
  TEST_CHECK(editor.current_buffer_index == 1);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_man_command_missing_page(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  setenv("MANPATH", manpath, 1);
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  enter_command(&editor, "man nosuch");
  TEST_CHECK(editor.number_of_buffers == 1);
  TEST_CHECK(editor.error_message != NULL &&
             strcmp(editor.error_message, "No manual entry for nosuch") == 0);
  TEST_MSG("message: %s", editor.error_message ? editor.error_message : "(none)");
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_man_command_without_name_opens_whatis(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d2/whatis"), "printf - formatted output\n");
  setenv("MANPATH", manpath, 1);
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  enter_command(&editor, "man");
  TEST_CHECK(strcmp(current_name(&editor), path_in_dir("d2/whatis")) == 0);
  TEST_CHECK(editor.current_buffer->autosave_disabled);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_man_command_gzipped_page_through_zcat(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  char name[64];
  snprintf(name, sizeof(name), "yasvitest%d", (int)getpid());
  char page[96];
  snprintf(page, sizeof(page), "d2/%s.txt.gz", name);
  write_gzipped(path_in_dir(page), "GZIPPED PAGE\nsecond line\n");
  char alias_line[160];
  snprintf(alias_line, sizeof(alias_line), "other%s %s\n", name, name);
  write_file(path_in_dir("d2/aliases"), alias_line);
  setenv("MANPATH", manpath, 1);
  char copy[128];
  snprintf(copy, sizeof(copy), "%s/%s%s.txt", MAN_TEMP_DIR, MAN_TEMP_PREFIX, name);

  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  char command[128];
  snprintf(command, sizeof(command), "man %s", name);
  enter_command(&editor, command);
  TEST_CHECK(editor.number_of_buffers == 2);
  TEST_CHECK(strcmp(current_name(&editor), copy) == 0);
  TEST_MSG("buffer: %s", current_name(&editor));
  TEST_CHECK(strcmp(first_row(&editor), "GZIPPED PAGE") == 0);
  TEST_CHECK(editor.current_buffer->autosave_disabled);

  // The alias names the same page: same copy, same buffer.
  editor_switch_to_buffer_by_index(&editor, 0);
  snprintf(command, sizeof(command), "man other%s", name);
  enter_command(&editor, command);
  TEST_CHECK(editor.number_of_buffers == 2);
  TEST_CHECK(editor.current_buffer_index == 1);
  editor_deinit(&editor);
  remove(copy);
  remove_temp_dir();
}

void test_man_index_lookup(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d1/index"), "alphabet b03.xz 1 2\nalpha b01.xz 10 20\n");
  write_file(path_in_dir("d2/alpha.txt"), "plain\n");
  write_file(path_in_dir("d1/aliases"), "beta alpha\n");
  ManPage page;
  // d1 comes first: its index entry wins over d2's plain file.
  TEST_ASSERT(man_locate(manpath, "alpha", &page));
  TEST_CHECK(strcmp(page.file, path_in_dir("d1/b01.xz")) == 0);
  TEST_CHECK(strcmp(page.name, "alpha") == 0);
  TEST_CHECK(page.offset == 10 && page.length == 20);
  man_page_free(&page);
  // An alias resolves through the index too, keeping the page's own name.
  TEST_ASSERT(man_locate(manpath, "beta", &page));
  TEST_CHECK(strcmp(page.name, "alpha") == 0 && page.offset == 10);
  man_page_free(&page);
  // Within one directory a plain file goes before the index.
  write_file(path_in_dir("d1/alpha.txt"), "plain in d1\n");
  TEST_ASSERT(man_locate(manpath, "alpha", &page));
  TEST_CHECK(strcmp(page.file, path_in_dir("d1/alpha.txt")) == 0);
  TEST_CHECK(page.offset < 0);
  man_page_free(&page);
  check_path(man_find_page(manpath, "alphabet"), path_in_dir("d1/b03.xz"));
  TEST_CHECK(!man_locate(manpath, "alph", &page));
  remove_temp_dir();
}

void test_man_cut(void) {
  make_temp_dir();
  const char* path = path_in_dir("block");
  write_file(path, "AAAAthe pageBBBB");
  TEST_CHECK(man_cut(path, 4, 8));
  FILE* f = fopen(path, "r");
  char text[32] = {0};
  TEST_ASSERT(f != NULL);
  TEST_CHECK(fread(text, 1, sizeof(text) - 1, f) == 8);
  fclose(f);
  TEST_CHECK(strcmp(text, "the page") == 0);
  TEST_CHECK(!man_cut(path, 4, 100));  // past the end
  TEST_CHECK(!man_cut(path_in_dir("missing"), 0, 1));
  remove_temp_dir();
}

void test_man_command_packed_page_through_xzcat(void) {
  if (system("command -v xz > /dev/null && test -x /usr/bin/xzcat") != 0) {
    TEST_MSG("no xz/xzcat on this host: skipped");
    return;
  }
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  char name[64];
  snprintf(name, sizeof(name), "yasvixz%d", (int)getpid());
  // A block of two pages, as build_man.py packs them: FIRST (6 bytes), then
  // the page under test.
  write_file(path_in_dir("d2/block.plain"), "FIRST\nPACKED PAGE\nsecond line\n");
  char cmd[700];
  snprintf(cmd, sizeof(cmd), "xz -c %s > %s && rm %s", path_in_dir("d2/block.plain"),
           path_in_dir("d2/b00.xz"), path_in_dir("d2/block.plain"));
  TEST_ASSERT(system(cmd) == 0);
  char index[160];
  snprintf(index, sizeof(index), "first b00.xz 0 6\n%s b00.xz 6 24\n", name);
  write_file(path_in_dir("d2/index"), index);
  setenv("MANPATH", manpath, 1);
  char copy[128];
  snprintf(copy, sizeof(copy), "%s/%s%s.txt", MAN_TEMP_DIR, MAN_TEMP_PREFIX, name);

  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  char command[128];
  snprintf(command, sizeof(command), "man %s", name);
  enter_command(&editor, command);
  TEST_CHECK(editor.number_of_buffers == 2);
  TEST_CHECK(strcmp(current_name(&editor), copy) == 0);
  TEST_MSG("buffer: %s", current_name(&editor));
  TEST_CHECK(strcmp(first_row(&editor), "PACKED PAGE") == 0);
  TEST_MSG("first row: %s", first_row(&editor));
  TEST_CHECK(editor.current_buffer->autosave_disabled);
  editor_deinit(&editor);
  remove(copy);
  remove_temp_dir();
}

// Opens a file holding `line` and puts the cursor `column` characters in.
static void editor_on_line(Editor* editor, const char* line, int column) {
  char text[200];
  snprintf(text, sizeof(text), "%s\n", line);
  write_file(path_in_dir("code.c"), text);
  setup_editor(editor);
  editor_load_file(editor, path_in_dir("code.c"));
  for (int i = 0; i < column; ++i) {
    editor_process_key(editor, 'l');
  }
}

void test_man_k_opens_header_page(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d1/stdio.h.txt"), "STDIO.H\n");
  write_file(path_in_dir("d1/aliases"), "sys/stat.h sys-stat.h\n");
  write_file(path_in_dir("d1/sys-stat.h.txt"), "SYS-STAT.H\n");
  setenv("MANPATH", manpath, 1);

  Editor editor;
  editor_on_line(&editor, "#include <stdio.h>", 12);  // on the 'i' of stdio
  editor_process_key(&editor, 'K');
  TEST_CHECK(strcmp(current_name(&editor), path_in_dir("d1/stdio.h.txt")) == 0);
  TEST_MSG("buffer: %s", current_name(&editor));
  editor_deinit(&editor);

  editor_on_line(&editor, "#include <sys/stat.h>", 14);  // on "stat"
  editor_process_key(&editor, 'K');
  TEST_CHECK(strcmp(current_name(&editor), path_in_dir("d1/sys-stat.h.txt")) == 0);
  TEST_MSG("buffer: %s", current_name(&editor));
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_man_k_falls_back_to_identifier(void) {
  make_temp_dir();
  char manpath[200];
  make_man_dirs(manpath, sizeof(manpath));
  write_file(path_in_dir("d1/printf.txt"), "PRINTF\n");
  setenv("MANPATH", manpath, 1);

  Editor editor;
  // "ops.printf" has no page; the identifier under the cursor does.
  editor_on_line(&editor, "  ops.printf(\"x\");", 7);
  editor_process_key(&editor, 'K');
  TEST_CHECK(strcmp(current_name(&editor), path_in_dir("d1/printf.txt")) == 0);
  editor_deinit(&editor);

  // A sentence's full stop is not part of the word.
  editor_on_line(&editor, "see printf.", 5);
  editor_process_key(&editor, 'K');
  TEST_CHECK(strcmp(current_name(&editor), path_in_dir("d1/printf.txt")) == 0);
  editor_deinit(&editor);

  editor_on_line(&editor, "x = nosuchfn();", 6);
  editor_process_key(&editor, 'K');
  TEST_CHECK(editor.number_of_buffers == 1);
  TEST_CHECK(editor.error_message != NULL &&
             strstr(editor.error_message, "No manual entry for nosuchfn") != NULL);
  editor_deinit(&editor);
  remove_temp_dir();
}

TEST_LIST = {
  {"man_find_plain_page", test_man_find_plain_page},
  {"man_find_gzipped_page", test_man_find_gzipped_page},
  {"man_plain_before_gzipped_in_one_dir", test_man_plain_before_gzipped_in_one_dir},
  {"man_directory_order_wins_over_compression",
   test_man_directory_order_wins_over_compression},
  {"man_aliases", test_man_aliases},
  {"man_pages_before_aliases", test_man_pages_before_aliases},
  {"man_alias_to_missing_page_tries_next_dir",
   test_man_alias_to_missing_page_tries_next_dir},
  {"man_empty_entries_and_default", test_man_empty_entries_and_default},
  {"man_whatis", test_man_whatis},
  {"man_temp_path_and_gzip_check", test_man_temp_path_and_gzip_check},
  {"man_command_opens_and_reuses_page", test_man_command_opens_and_reuses_page},
  {"man_command_missing_page", test_man_command_missing_page},
  {"man_command_without_name_opens_whatis",
   test_man_command_without_name_opens_whatis},
  {"man_command_gzipped_page_through_zcat",
   test_man_command_gzipped_page_through_zcat},
  {"man_index_lookup", test_man_index_lookup},
  {"man_cut", test_man_cut},
  {"man_command_packed_page_through_xzcat", test_man_command_packed_page_through_xzcat},
  {"man_k_opens_header_page", test_man_k_opens_header_page},
  {"man_k_falls_back_to_identifier", test_man_k_falls_back_to_identifier},
  {NULL, NULL}};
