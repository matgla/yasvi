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

// fileops.c (paths, file operations, completion), the file tree's refresh
// and navigation, and through the editor: :e/:open, the tree's a/A/r/c/d
// keys and Tab completion on the command line.

#define _GNU_SOURCE

#include "acutest.h"

#include "mock_ncurses.h"

#include "editor.h"
#include "file_manager.h"
#include "fileops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_dir[64];

static void make_temp_dir(void) {
  snprintf(g_dir, sizeof(g_dir), "/tmp/yasvi_fileops_XXXXXX");
  TEST_ASSERT(mkdtemp(g_dir) != NULL);
}

static void remove_temp_dir(void) {
  char cmd[160];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
  TEST_CHECK(system(cmd) == 0);
}

static char* in_dir(const char* name) {
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
  static char text[4096];
  FILE* f = fopen(path, "r");
  if (f == NULL) {
    return NULL;
  }
  size_t n = fread(text, 1, sizeof(text) - 1, f);
  text[n] = '\0';
  fclose(f);
  return text;
}

static bool exists(const char* path) {
  struct stat st;
  return lstat(path, &st) == 0;
}

static bool is_dir(const char* path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void check_string(const char* actual, const char* expected) {
  TEST_CHECK(actual != NULL && strcmp(actual, expected) == 0);
  TEST_MSG("got      [%s]", actual != NULL ? actual : "(null)");
  TEST_MSG("expected [%s]", expected);
}

static void check_owned(char* actual, const char* expected) {
  check_string(actual, expected);
  free(actual);
}

/* ---- paths ---------------------------------------------------------------- */

void test_path_helpers(void) {
  check_owned(path_join("/", "a"), "/a");
  check_owned(path_join("/x/", "a"), "/x/a");
  check_owned(path_join("x", "a/b"), "x/a/b");
  check_owned(path_absolute("/a/./b//c/../d"), "/a/b/d");
  check_owned(path_absolute("/.."), "/");
  check_owned(path_absolute("/a/b/../.."), "/");
  check_owned(path_dirname("x"), ".");
  check_owned(path_dirname("/x"), "/");
  check_owned(path_dirname("/a/b"), "/a");
  check_string(path_basename("/a/b"), "b");
  check_string(path_basename("/a/dir/"), "dir/");
  check_string(path_basename("plain"), "plain");
  check_owned(path_resolve("/d", "x"), "/d/x");
  check_owned(path_resolve("/d", "/y"), "/y");

  setenv("HOME", "/home/someone", 1);
  check_owned(path_expand_home("~"), "/home/someone");
  check_owned(path_expand_home("~/notes.txt"), "/home/someone/notes.txt");
  check_owned(path_expand_home("~x"), "~x");

  TEST_CHECK(path_is_within("/a/b/c", "/a/b"));
  TEST_CHECK(path_is_within("/a/b", "/a/b"));
  TEST_CHECK(!path_is_within("/a/bc", "/a/b"));
  TEST_CHECK(path_is_within("/anything", "/"));

  make_temp_dir();
  char cwd[512];
  TEST_ASSERT(getcwd(cwd, sizeof(cwd)) != NULL);
  TEST_ASSERT(chdir(g_dir) == 0);
  check_owned(path_absolute("x/../y.c"), in_dir("y.c"));
  TEST_CHECK(path_same("y.c", in_dir("y.c")));
  TEST_CHECK(path_same("./y.c", in_dir("y.c")));
  TEST_CHECK(!path_same("z.c", in_dir("y.c")));
  TEST_ASSERT(chdir(cwd) == 0);
  remove_temp_dir();
}

/* ---- operations ----------------------------------------------------------- */

void test_file_operations(void) {
  make_temp_dir();
  // create
  TEST_CHECK(fileops_create_file(in_dir("a.txt")) == NULL);
  TEST_CHECK(exists(in_dir("a.txt")));
  const char* error = fileops_create_file(in_dir("a.txt"));
  TEST_CHECK(error != NULL && strstr(error, "already exists") != NULL);
  TEST_CHECK(fileops_make_directory(in_dir("p/q/r")) == NULL);
  TEST_CHECK(is_dir(in_dir("p/q/r")));

  // rename, never over an existing name
  write_file(in_dir("a.txt"), "alpha\n");
  TEST_CHECK(fileops_rename(in_dir("a.txt"), in_dir("b.txt")) == NULL);
  TEST_CHECK(!exists(in_dir("a.txt")));
  check_string(read_file(in_dir("b.txt")), "alpha\n");
  write_file(in_dir("c.txt"), "gamma\n");
  error = fileops_rename(in_dir("b.txt"), in_dir("c.txt"));
  TEST_CHECK(error != NULL && strstr(error, "already exists") != NULL);
  check_string(read_file(in_dir("c.txt")), "gamma\n");
  TEST_CHECK(fileops_rename(in_dir("p"), in_dir("p/q/inside")) != NULL);

  // copy a file and a tree
  TEST_CHECK(fileops_copy(in_dir("b.txt"), in_dir("b2.txt")) == NULL);
  check_string(read_file(in_dir("b2.txt")), "alpha\n");
  write_file(in_dir("p/q/deep.txt"), "deep\n");
  TEST_CHECK(fileops_copy(in_dir("p"), in_dir("p2")) == NULL);
  check_string(read_file(in_dir("p2/q/deep.txt")), "deep\n");
  TEST_CHECK(is_dir(in_dir("p2/q/r")));
  error = fileops_copy(in_dir("p"), in_dir("p/q/again"));
  TEST_CHECK(error != NULL && strstr(error, "into itself") != NULL);
  TEST_CHECK(fileops_copy(in_dir("b.txt"), in_dir("c.txt")) != NULL);

  // remove a tree
  TEST_CHECK(fileops_remove(in_dir("p2")) == NULL);
  TEST_CHECK(!exists(in_dir("p2")));
  TEST_CHECK(fileops_remove(in_dir("b2.txt")) == NULL);
  TEST_CHECK(!exists(in_dir("b2.txt")));
  TEST_CHECK(fileops_remove(in_dir("missing")) != NULL);
  error = fileops_remove("/");
  TEST_CHECK(error != NULL && strstr(error, "Refusing") != NULL);
  remove_temp_dir();
}

void test_path_completion(void) {
  make_temp_dir();
  write_file(in_dir("alpha.c"), "");
  TEST_ASSERT(mkdir(in_dir("alpine"), 0755) == 0);
  write_file(in_dir("beta.c"), "");
  write_file(in_dir(".hidden"), "");

  char partial[320];
  snprintf(partial, sizeof(partial), "%s/al", g_dir);
  char** matches = NULL;
  int count = path_complete(partial, &matches);
  TEST_ASSERT(count == 2);
  check_string(matches[0], in_dir("alpha.c"));
  check_string(matches[1], in_dir("alpine/"));
  check_owned(path_common_prefix(matches, count), in_dir("alp"));
  path_matches_free(matches, count);

  snprintf(partial, sizeof(partial), "%s/", g_dir);
  count = path_complete(partial, &matches);
  TEST_CHECK(count == 3);  // .hidden left out
  path_matches_free(matches, count);

  snprintf(partial, sizeof(partial), "%s/.h", g_dir);
  count = path_complete(partial, &matches);
  TEST_CHECK(count == 1);
  path_matches_free(matches, count);

  snprintf(partial, sizeof(partial), "%s/zzz", g_dir);
  TEST_CHECK(path_complete(partial, &matches) == 0);
  TEST_CHECK(matches == NULL);
  remove_temp_dir();
}

/* ---- the tree ------------------------------------------------------------- */

// The cursor's line must be its place among the shown entries.
static int visible_index(FileManager* fm, const char* name) {
  // Walk the shown entries the way the tree draws them.
  int line = 0;
  FileManagerEntry* stack[64];
  int depth = 0;
  stack[depth++] = fm->root;
  while (depth > 0) {
    FileManagerEntry* entry = stack[--depth];
    if (strcmp(entry->name, name) == 0) {
      return line;
    }
    ++line;
    if (entry->type == FileManagerEntryType_Directory && entry->expanded) {
      FileManagerEntry* children[64];
      int n = 0;
      for (FileManagerEntry* c = entry->children; c != NULL && n < 64; c = c->next) {
        children[n++] = c;
      }
      while (n > 0 && depth < 64) {
        stack[depth++] = children[--n];
      }
    }
  }
  return -1;
}

void test_tree_go_to_parent_keeps_cursor_in_step(void) {
  make_temp_dir();
  TEST_ASSERT(mkdir(in_dir("sub"), 0755) == 0);
  write_file(in_dir("sub/inner.txt"), "x\n");
  write_file(in_dir("top.txt"), "y\n");

  FileManager* fm = file_manager_alloc();
  TEST_ASSERT(file_manager_init(fm, in_dir("sub")));
  file_manager_go_to_parent(fm);
  check_string(fm->root->full_path, g_dir);
  TEST_ASSERT(fm->cursor != NULL);
  check_string(fm->cursor->name, "sub");
  TEST_CHECK(fm->cursor->expanded);
  TEST_CHECK(fm->cursor_line == visible_index(fm, "sub"));

  file_manager_move_cursor_down(fm);
  check_string(fm->cursor->name, "inner.txt");
  TEST_CHECK(fm->cursor_line == visible_index(fm, "inner.txt"));
  file_manager_move_cursor_down(fm);
  check_string(fm->cursor->name, "top.txt");
  TEST_CHECK(fm->cursor_line == visible_index(fm, "top.txt"));

  // Collapsing the directory the cursor came out of keeps the line right.
  TEST_CHECK(file_manager_select_path(fm, in_dir("sub")));
  file_manager_toggle_expand(fm);
  file_manager_move_cursor_down(fm);
  check_string(fm->cursor->name, "top.txt");
  TEST_CHECK(fm->cursor_line == 2);
  file_manager_free(fm);
  remove_temp_dir();
}

void test_tree_refresh(void) {
  make_temp_dir();
  TEST_ASSERT(mkdir(in_dir("sub"), 0755) == 0);
  write_file(in_dir("sub/f.txt"), "");
  FileManager* fm = file_manager_alloc();
  TEST_ASSERT(file_manager_init(fm, g_dir));
  TEST_CHECK(file_manager_select_path(fm, in_dir("sub/f.txt")));
  TEST_CHECK(visible_index(fm, "new.txt") < 0);

  write_file(in_dir("new.txt"), "");
  write_file(in_dir("sub/g.txt"), "");
  file_manager_refresh(fm);
  TEST_CHECK(visible_index(fm, "new.txt") > 0);
  TEST_CHECK(visible_index(fm, "g.txt") > 0);  // sub is still expanded
  check_string(fm->cursor->name, "f.txt");
  TEST_CHECK(fm->cursor_line == visible_index(fm, "f.txt"));

  // The selected file deleted elsewhere: the cursor goes to its directory.
  TEST_CHECK(unlink(in_dir("sub/f.txt")) == 0);
  file_manager_refresh(fm);
  check_string(fm->cursor->name, "sub");

  char* target = file_manager_target_directory(fm);
  check_owned(target, in_dir("sub"));
  TEST_CHECK(file_manager_select_path(fm, in_dir("new.txt")));
  check_owned(file_manager_target_directory(fm), g_dir);
  file_manager_free(fm);
  remove_temp_dir();
}

/* ---- through the editor --------------------------------------------------- */

static void setup_editor(Editor* editor) {
  memset(editor, 0, sizeof(*editor));
  editor->state = EditorState_Running;
  editor->tab_size = 4;
  editor->window.width = 80;
  editor->window.height = 24;
  editor->number_of_line_digits = 4;
  editor->cursor.x = editor->number_of_line_digits;
  editor->cursor.y = 2;
  editor->file_manager = file_manager_alloc();
  TEST_ASSERT(editor->file_manager != NULL);
  TEST_ASSERT(file_manager_init(editor->file_manager, g_dir));
}

static void keys(Editor* editor, const char* text) {
  for (const char* p = text; *p != '\0'; ++p) {
    editor_process_key(editor, (unsigned char)*p);
  }
}

static void enter_command(Editor* editor, const char* cmd) {
  editor_process_key(editor, ':');
  keys(editor, cmd);
  editor_process_key(editor, '\n');
}

static const char* message(const Editor* editor) {
  return editor->error_message != NULL ? editor->error_message : "";
}

static const char* current_name(const Editor* editor) {
  const char* name = buffer_get_filename(editor->current_buffer);
  return name != NULL ? name : "";
}

void test_edit_command(void) {
  make_temp_dir();
  write_file(in_dir("a.c"), "int a;\n");
  write_file(in_dir("b.c"), "int b;\nint c;\n");
  TEST_ASSERT(mkdir(in_dir("dir"), 0755) == 0);
  char cwd[512];
  TEST_ASSERT(getcwd(cwd, sizeof(cwd)) != NULL);

  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  char command[400];
  snprintf(command, sizeof(command), "e %s", in_dir("a.c"));
  enter_command(&editor, command);
  TEST_CHECK(editor.number_of_buffers == 1);  // the blank buffer is replaced
  check_string(current_name(&editor), in_dir("a.c"));
  TEST_CHECK(strstr(message(&editor), "\"a.c\" 1 lines") != NULL);
  TEST_MSG("message: %s", message(&editor));

  snprintf(command, sizeof(command), "open %s", in_dir("b.c"));
  enter_command(&editor, command);
  TEST_CHECK(editor.number_of_buffers == 2);
  check_string(current_name(&editor), in_dir("b.c"));

  // Another spelling of an open file switches to it.
  TEST_ASSERT(chdir(g_dir) == 0);
  enter_command(&editor, "e ./x/../a.c");
  TEST_CHECK(editor.number_of_buffers == 2);
  check_string(current_name(&editor), in_dir("a.c"));
  enter_command(&editor, "e new.c");
  TEST_CHECK(editor.number_of_buffers == 3);
  TEST_CHECK(strstr(message(&editor), "[New]") != NULL);
  TEST_CHECK(!exists(in_dir("new.c")));  // :w creates it
  TEST_ASSERT(chdir(cwd) == 0);

  enter_command(&editor, "open");
  TEST_CHECK(strstr(message(&editor), "Usage: :open FILE") != NULL);

  // A directory opens in the tree.
  snprintf(command, sizeof(command), "e %s", in_dir("dir"));
  enter_command(&editor, command);
  TEST_CHECK(editor.state == EditorState_FileManager);
  TEST_CHECK(file_manager_is_visible(editor.file_manager));
  check_string(editor.file_manager->root->full_path, in_dir("dir"));
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_edit_reload(void) {
  make_temp_dir();
  write_file(in_dir("a.c"), "one\n");
  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, in_dir("a.c"));
  keys(&editor, "ix");
  editor_process_key(&editor, 27);
  TEST_CHECK(buffer_is_modified(editor.current_buffer));
  enter_command(&editor, "e");
  TEST_CHECK(strstr(message(&editor), "No write since last change") != NULL);
  write_file(in_dir("a.c"), "two\nthree\n");
  enter_command(&editor, "e!");
  TEST_CHECK(!buffer_is_modified(editor.current_buffer));
  TEST_CHECK(buffer_get_number_of_lines(editor.current_buffer) == 2);
  check_string(buffer_get_first_row(editor.current_buffer)->data, "two");
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_tree_file_operations(void) {
  make_temp_dir();
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  FileManager* fm = editor.file_manager;

  editor_process_key(&editor, 2);  // Ctrl-B
  TEST_ASSERT(editor.state == EditorState_FileManager);

  // a: a new file is made and opened, the keys go to it
  keys(&editor, "an.txt\n");
  TEST_CHECK(exists(in_dir("n.txt")));
  check_string(current_name(&editor), in_dir("n.txt"));
  TEST_CHECK(editor.state == EditorState_Running);
  TEST_CHECK(file_manager_is_visible(fm));
  check_string(fm->cursor->name, "n.txt");

  // r: rename; the open buffer follows
  editor_process_key(&editor, 8);  // Ctrl-H
  TEST_ASSERT(editor.state == EditorState_FileManager);
  editor_process_key(&editor, 'r');
  check_string(fm->prompt_input, "n.txt");
  editor_process_key(&editor, 0x15);  // Ctrl-U
  keys(&editor, "m.txt\n");
  TEST_CHECK(!exists(in_dir("n.txt")));
  TEST_CHECK(exists(in_dir("m.txt")));
  check_string(current_name(&editor), in_dir("m.txt"));
  check_string(fm->cursor->name, "m.txt");

  // c: copy
  keys(&editor, "c");
  editor_process_key(&editor, 0x15);
  keys(&editor, "copy.txt\n");
  TEST_CHECK(exists(in_dir("copy.txt")));
  TEST_CHECK(exists(in_dir("m.txt")));

  // A: a directory, then r into it moves the file there
  keys(&editor, "Anewdir\n");
  TEST_CHECK(is_dir(in_dir("newdir")));
  TEST_ASSERT(file_manager_select_path(fm, in_dir("m.txt")));
  keys(&editor, "r");
  editor_process_key(&editor, 0x15);
  keys(&editor, "newdir\n");
  TEST_CHECK(exists(in_dir("newdir/m.txt")));
  check_string(current_name(&editor), in_dir("newdir/m.txt"));
  check_string(fm->cursor->name, "m.txt");
  TEST_CHECK(fm->cursor->parent != NULL && strcmp(fm->cursor->parent->name, "newdir") == 0);

  // a with a trailing slash makes a directory; Escape drops a prompt
  TEST_ASSERT(file_manager_select_path(fm, g_dir));
  keys(&editor, "aother/\n");
  TEST_CHECK(is_dir(in_dir("other")));
  keys(&editor, "ajunk");
  editor_process_key(&editor, 27);
  TEST_CHECK(fm->prompt == FileManagerPrompt_None);
  TEST_CHECK(!exists(in_dir("junk")));
  TEST_CHECK(editor.state == EditorState_FileManager);

  // d: only y deletes
  TEST_ASSERT(file_manager_select_path(fm, in_dir("copy.txt")));
  keys(&editor, "dn");
  TEST_CHECK(exists(in_dir("copy.txt")));
  keys(&editor, "dy");
  TEST_CHECK(!exists(in_dir("copy.txt")));
  TEST_CHECK(strstr(message(&editor), "Deleted copy.txt") != NULL);
  TEST_ASSERT(file_manager_select_path(fm, in_dir("newdir")));
  keys(&editor, "dy");
  TEST_CHECK(!exists(in_dir("newdir")));
  TEST_CHECK(strstr(message(&editor), "still open") != NULL);
  TEST_MSG("message: %s", message(&editor));

  // R reads the tree again: a file made behind its back shows up
  write_file(in_dir("outside.txt"), "");
  keys(&editor, "R");
  TEST_CHECK(file_manager_select_path(fm, in_dir("outside.txt")));
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_tree_shows_new_files_when_opened(void) {
  make_temp_dir();
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  write_file(in_dir("later.txt"), "");
  editor_process_key(&editor, 2);  // Ctrl-B: read again when shown
  TEST_CHECK(file_manager_select_path(editor.file_manager, in_dir("later.txt")));
  editor_process_key(&editor, 27);
  editor_process_key(&editor, 2);  // closed
  write_file(in_dir("even_later.txt"), "");
  editor_process_key(&editor, 8);  // Ctrl-H shows it again
  TEST_CHECK(file_manager_select_path(editor.file_manager, in_dir("even_later.txt")));

  // :w of a new file while the tree is shown
  editor_process_key(&editor, 27);
  char command[400];
  snprintf(command, sizeof(command), "w %s", in_dir("saved.txt"));
  enter_command(&editor, command);
  TEST_CHECK(exists(in_dir("saved.txt")));
  TEST_CHECK(file_manager_select_path(editor.file_manager, in_dir("saved.txt")));
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_command_line_tab_completion(void) {
  make_temp_dir();
  write_file(in_dir("alpha.c"), "");
  write_file(in_dir("alpine.c"), "");
  write_file(in_dir("zeta.c"), "");
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);

  editor_process_key(&editor, ':');
  char typed[320];
  snprintf(typed, sizeof(typed), "e %s/al", g_dir);
  keys(&editor, typed);
  editor_process_key(&editor, '\t');
  char expected[320];
  snprintf(expected, sizeof(expected), "e %s/alp", g_dir);
  check_string(editor.command.buffer, expected);
  editor_process_key(&editor, '\t');  // nothing more in common: the names
  TEST_CHECK(strstr(message(&editor), "alpha.c alpine.c") != NULL);
  TEST_MSG("message: %s", message(&editor));
  keys(&editor, "h");
  editor_process_key(&editor, '\t');
  snprintf(expected, sizeof(expected), "e %s/alpha.c", g_dir);
  check_string(editor.command.buffer, expected);
  editor_process_key(&editor, '\n');
  check_string(current_name(&editor), in_dir("alpha.c"));
  editor_deinit(&editor);
  remove_temp_dir();
}

TEST_LIST = {
  {"path_helpers", test_path_helpers},
  {"file_operations", test_file_operations},
  {"path_completion", test_path_completion},
  {"tree_go_to_parent_keeps_cursor_in_step", test_tree_go_to_parent_keeps_cursor_in_step},
  {"tree_refresh", test_tree_refresh},
  {"edit_command", test_edit_command},
  {"edit_reload", test_edit_reload},
  {"tree_file_operations", test_tree_file_operations},
  {"tree_shows_new_files_when_opened", test_tree_shows_new_files_when_opened},
  {"command_line_tab_completion", test_command_line_tab_completion},
  {NULL, NULL}};
