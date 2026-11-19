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

// indent.c (block openers, bracket matching, the indent step) and through
// the editor: Enter, o, O, a typed closing bracket, Tab and Backspace in the
// indentation with autoindent on, and Backspace joining lines.

#define _GNU_SOURCE

#include "acutest.h"

#include "mock_ncurses.h"

#include "editor.h"
#include "file_manager.h"
#include "indent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_dir[64];

static void make_temp_dir(void) {
  snprintf(g_dir, sizeof(g_dir), "/tmp/yasvi_indent_XXXXXX");
  TEST_ASSERT(mkdtemp(g_dir) != NULL);
}

static void remove_temp_dir(void) {
  char cmd[160];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
  TEST_CHECK(system(cmd) == 0);
}

static char* in_dir(const char* name) {
  static char paths[4][300];
  static int next;
  char* out = paths[next++ % 4];
  snprintf(out, 300, "%s/%s", g_dir, name);
  return out;
}

static void write_file(const char* path, const char* text) {
  FILE* f = fopen(path, "w");
  TEST_ASSERT(f != NULL);
  fputs(text, f);
  fclose(f);
}

static void check_string(const char* actual, const char* expected) {
  TEST_CHECK(actual != NULL && strcmp(actual, expected) == 0);
  TEST_MSG("expected \"%s\", got \"%s\"", expected, actual ? actual : "(null)");
}

static void setup_editor(Editor* editor) {
  memset(editor, 0, sizeof(*editor));
  editor->state = EditorState_Running;
  editor->tab_size = 2;
  editor->window.width = 80;
  editor->window.height = 24;
  editor->number_of_line_digits = 4;
  editor->cursor.x = editor->number_of_line_digits;
  editor->cursor.y = 2;
  editor->config.autoindent = true;
  editor->file_manager = file_manager_alloc();
  TEST_ASSERT(editor->file_manager != NULL);
  TEST_ASSERT(file_manager_init(editor->file_manager, g_dir));
}

// Opens NAME holding TEXT in a fresh editor.
static void open_file(Editor* editor, const char* name, const char* text) {
  write_file(in_dir(name), text);
  setup_editor(editor);
  editor_load_file(editor, in_dir(name));
  TEST_ASSERT(editor->current_buffer != NULL);
}

static void keys(Editor* editor, const char* text) {
  for (const char* p = text; *p != '\0'; ++p) {
    editor_process_key(editor, (unsigned char)*p);
  }
}

// The buffer's lines joined with '\n'.
static const char* text_of(const Editor* editor) {
  static char out[4096];
  size_t used = 0;
  out[0] = '\0';
  for (const BufferRow* row = buffer_get_first_row(editor->current_buffer); row != NULL;
       row = row->next) {
    used += (size_t)snprintf(out + used, sizeof(out) - used, "%s%s",
                             row == buffer_get_first_row(editor->current_buffer) ? "" : "\n",
                             row->data);
  }
  return out;
}

static int line_of(const Editor* editor) {
  return buffer_get_current_line_number(editor->current_buffer);
}

/* ---- indent.c ------------------------------------------------------------ */

void test_indent_rules(void) {
  TEST_CHECK(indent_language("c") == IndentLanguage_Brackets);
  TEST_CHECK(indent_language("zig") == IndentLanguage_Brackets);
  TEST_CHECK(indent_language("shell") == IndentLanguage_Shell);
  TEST_CHECK(indent_language(NULL) == IndentLanguage_None);
  TEST_CHECK(indent_leading("  \tx", 4) == 3);
  TEST_CHECK(indent_leading("   ", 3) == 3);

  const IndentLanguage c = IndentLanguage_Brackets;
#define OPENS(lang, s) indent_opens_block(lang, s, (int)strlen(s))
  TEST_CHECK(OPENS(c, "int main() {"));
  TEST_CHECK(OPENS(c, "foo(  "));
  TEST_CHECK(!OPENS(c, "foo(a,  "));
  TEST_CHECK(OPENS(c, "int x[] = {  // values"));
  TEST_CHECK(!OPENS(c, "x = 1;"));
  TEST_CHECK(!OPENS(c, "x = 1; // {"));
  TEST_CHECK(!OPENS(c, "s = \"{\";"));
  TEST_CHECK(!OPENS(c, "c = '{'"));
  TEST_CHECK(!OPENS(c, "\\\\ a zig string {"));
  TEST_CHECK(!OPENS(IndentLanguage_None, "int main() {"));

  const IndentLanguage sh = IndentLanguage_Shell;
  TEST_CHECK(OPENS(sh, "if [ -f x ]; then"));
  TEST_CHECK(OPENS(sh, "for i in a b; do"));
  TEST_CHECK(OPENS(sh, "else"));
  TEST_CHECK(OPENS(sh, "case $x in"));
  TEST_CHECK(OPENS(sh, "f() {"));
  TEST_CHECK(!OPENS(sh, "for x in a"));
  TEST_CHECK(!OPENS(sh, "echo undo"));
  TEST_CHECK(!OPENS(sh, "echo x # then"));
  TEST_CHECK(!OPENS(sh, "echo 'do {'"));
#undef OPENS

  TEST_CHECK(indent_starts_with_closer("  }", 3));
  TEST_CHECK(indent_starts_with_closer(");", 2));
  TEST_CHECK(!indent_starts_with_closer("  x }", 5));
}

void test_indent_find_opener_and_unit(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "a.c",
            "int main() {\n"
            "    if (x) {\n"
            "        s = \"}\"; f(a[1]);\n"
            "    }\n"
            "}\n");
  BufferRow* first = buffer_get_first_row(editor.current_buffer);
  BufferRow* closer = first->next->next->next;  // "    }"
  TEST_CHECK(indent_find_opener(IndentLanguage_Brackets, closer, 4) == first->next);
  TEST_CHECK(indent_find_opener(IndentLanguage_Brackets, closer->next, 0) == first);
  TEST_CHECK(indent_find_opener(IndentLanguage_Brackets, first, 0) == NULL);

  char unit[16];
  TEST_CHECK(indent_unit(first, 2, unit, sizeof(unit)) == 4);
  check_string(unit, "    ");
  editor_deinit(&editor);

  open_file(&editor, "b.c", "x;\n");  // nothing to go by: tab_size spaces
  TEST_CHECK(indent_unit(buffer_get_first_row(editor.current_buffer), 3, unit, 16) == 3);
  editor_deinit(&editor);

  open_file(&editor, "Makefile.c", "all:\n\tcc x.c\n\tcc y.c\n");
  TEST_CHECK(indent_unit(buffer_get_first_row(editor.current_buffer), 2, unit, 16) == 1);
  check_string(unit, "\t");
  editor_deinit(&editor);
  remove_temp_dir();
}

/* ---- through the editor -------------------------------------------------- */

void test_enter_keeps_and_adds_indentation(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "a.c", "int main() {\n  int a = 1;\n}\n");
  keys(&editor, "j$a\nint b = 2;");
  check_string(text_of(&editor), "int main() {\n  int a = 1;\n  int b = 2;\n}");
  // after a line ending in { one step more, and } goes back under its opener
  keys(&editor, "\nif (b) {\nb = 3;\n}");
  check_string(text_of(&editor),
               "int main() {\n  int a = 1;\n  int b = 2;\n  if (b) {\n    b = 3;\n  }\n}");
  TEST_CHECK(line_of(&editor) == 6);
  TEST_CHECK(editor_get_cursor_x(&editor) == 3);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_enter_between_braces(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "a.c", "void f() {}\n");
  keys(&editor, "$i\nreturn;");  // the cursor on }
  check_string(text_of(&editor), "void f() {\n  return;\n}");
  TEST_CHECK(line_of(&editor) == 2);
  editor_deinit(&editor);

  // text moved down loses its leading spaces, the old line its trailing ones
  open_file(&editor, "b.c", "  x = 1;   y = 2;\n");
  keys(&editor, "^");
  for (int i = 0; i < 9; ++i) {
    keys(&editor, "l");
  }
  keys(&editor, "i\n");
  check_string(text_of(&editor), "  x = 1;\n  y = 2;");
  TEST_CHECK(editor_get_cursor_x(&editor) == 2);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_escape_clears_unused_indentation(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "a.c", "  a;\n  b;\n");
  keys(&editor, "o\033");
  check_string(text_of(&editor), "  a;\n\n  b;");
  // Enter on a line of only indentation leaves it empty too
  keys(&editor, "ggo\n\033");
  check_string(text_of(&editor), "  a;\n\n\n\n  b;");
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_o_and_O(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "a.c", "if (x) {\n    y;\n}\n");
  keys(&editor, "oz;\033");
  check_string(text_of(&editor), "if (x) {\n    z;\n    y;\n}");
  keys(&editor, "GOw;\033");  // above a } : inside the block
  check_string(text_of(&editor), "if (x) {\n    z;\n    y;\n    w;\n}");
  keys(&editor, "ggOv;\033");
  check_string(text_of(&editor), "v;\nif (x) {\n    z;\n    y;\n    w;\n}");
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_tab_and_backspace_in_indentation(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "a.c", "a {\n    b;\n}\nx\n");
  keys(&editor, "Gi\t");  // to the next step of 4
  check_string(text_of(&editor), "a {\n    b;\n}\n    x");
  keys(&editor, " \t");  // 5 -> 8
  check_string(text_of(&editor), "a {\n    b;\n}\n        x");
  keys(&editor, "\177");  // 8 -> 4
  check_string(text_of(&editor), "a {\n    b;\n}\n    x");
  keys(&editor, "\177\177");  // 4 -> 0, then joins with the line above
  check_string(text_of(&editor), "a {\n    b;\n}x");
  TEST_CHECK(line_of(&editor) == 3);
  TEST_CHECK(editor_get_cursor_x(&editor) == 1);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_backspace_joins_at_the_line_end(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "a.c", "int main() {\n    int a = 1;\n    int b = 2;\n}\n");
  keys(&editor, "jji\177");
  check_string(text_of(&editor), "int main() {\n    int a = 1;    int b = 2;\n}");
  TEST_CHECK(line_of(&editor) == 2);
  TEST_CHECK(editor_get_cursor_x(&editor) == 14);  // where line 3 was put
  // the rows below were marked for drawing again
  BufferRow* last = buffer_get_first_row(editor.current_buffer)->next->next;
  TEST_CHECK(last != NULL && last->dirty);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_noautoindent_and_plain_text(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "a.c", "  if (x) {\n");
  editor.config.autoindent = false;
  keys(&editor, "$a\ny\t}");
  check_string(text_of(&editor), "  if (x) {\ny  }");
  editor_deinit(&editor);

  // plain text keeps the indentation but knows no brackets
  open_file(&editor, "notes.txt", "  list {\n");
  keys(&editor, "$a\nitem }");
  check_string(text_of(&editor), "  list {\n  item }");
  editor_deinit(&editor);

  // shell: then/do indent, fi is typed by hand
  open_file(&editor, "s.sh", "if true; then\n");
  keys(&editor, "$a\necho hi");
  check_string(text_of(&editor), "if true; then\n  echo hi");
  editor_deinit(&editor);
  remove_temp_dir();
}

// Makefiles keep hard tabs: Tab inserts a tab even in a file indented with
// spaces, Enter after a rule line starts its recipe with a tab, and a recipe
// line keeps it; a tab is drawn to the next multiple of 8 and the cursor goes
// after it.
void test_makefile_inserts_tabs(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "Makefile", "X = 1\nifdef A\n  Y = 2\nendif\n");
  TEST_CHECK(indent_language("make") == IndentLanguage_Make);
  keys(&editor, "Go\tcc\033");
  check_string(text_of(&editor), "X = 1\nifdef A\n  Y = 2\nendif\n\tcc");
  // Backspace takes the whole tab; Enter after a rule line gives a tab, a
  // recipe line keeps it
  keys(&editor, "o\177all: x\nmkdir -p y\nrm z\033");
  check_string(text_of(&editor),
               "X = 1\nifdef A\n  Y = 2\nendif\n\tcc\nall: x\n\tmkdir -p y\n\trm z");
  keys(&editor, "^i\177\033");
  check_string(text_of(&editor),
               "X = 1\nifdef A\n  Y = 2\nendif\n\tcc\nall: x\n\tmkdir -p y\nrm z");
  // after an assignment Enter adds nothing
  keys(&editor, "gg$a\nZ = 3\033");
  check_string(text_of(&editor),
               "X = 1\nZ = 3\nifdef A\n  Y = 2\nendif\n\tcc\nall: x\n\tmkdir -p y\nrm z");
  // a ) typed in a recipe stays where it is
  keys(&editor, "Gddo)\033");
  check_string(text_of(&editor),
               "X = 1\nZ = 3\nifdef A\n  Y = 2\nendif\n\tcc\nall: x\n\tmkdir -p y\n\t)");

  // drawn: the tab as spaces to column 8, the cursor after it on "m"
  keys(&editor, "k^");
  TEST_CHECK(editor_get_cursor_x(&editor) == 1);
  mock_ncurses_reset();
  for (BufferRow* row = buffer_get_first_row(editor.current_buffer); row != NULL;
       row = row->next) {
    row->dirty = true;
  }
  editor_redraw_screen(&editor);
  TEST_CHECK(strstr(mock_ncurses_get_added(), "        mkdir -p y") != NULL);
  TEST_MSG("drawn: \"%s\"", mock_ncurses_get_added());
  TEST_CHECK(mock_ncurses_get_last_x() == editor.number_of_line_digits + 8);
  TEST_MSG("cursor at %d", mock_ncurses_get_last_x());
  editor_deinit(&editor);

  // C files still get spaces
  open_file(&editor, "a.c", "x\n");
  keys(&editor, "o\ty\033");
  check_string(text_of(&editor), "x\n  y");
  editor_deinit(&editor);
  remove_temp_dir();
}

// :set tabstop redraws tabs that wide; :set expandtab / noexpandtab / et!
// and :set shiftwidth change the current file only; a file indented with
// tabs gets tab mode by itself; the status bar text follows.
void test_set_tabstop_expandtab_shiftwidth(void) {
  make_temp_dir();
  Editor editor;
  char text[32];
  open_file(&editor, "Makefile", "all:\n\tcc x\n");
  editor_describe_indent(&editor, text, sizeof(text));
  check_string(text, "tabs:8");
  keys(&editor, "j^");
  mock_ncurses_reset();
  keys(&editor, ":set ts=4\n");  // redraws every row, tabs 4 wide
  TEST_CHECK(strstr(mock_ncurses_get_added(), "    cc x") != NULL);
  TEST_CHECK(strstr(mock_ncurses_get_added(), "     cc x") == NULL);
  TEST_MSG("drawn: \"%s\"", mock_ncurses_get_added());
  editor_redraw_screen(&editor);
  TEST_CHECK(mock_ncurses_get_last_x() == editor.number_of_line_digits + 4);
  TEST_MSG("cursor at %d", mock_ncurses_get_last_x());
  editor_describe_indent(&editor, text, sizeof(text));
  check_string(text, "tabs:4");
  keys(&editor, ":set ts=0\n");  // refused, the value stays
  TEST_CHECK(editor.config.tabstop == 4);

  // spaces in this Makefile, then back; et! turns it over
  keys(&editor, ":set et\n");
  editor_describe_indent(&editor, text, sizeof(text));
  check_string(text, "spaces:2");  // tab_size: the file says nothing of spaces
  keys(&editor, "o\177\tz\033");
  check_string(text_of(&editor), "all:\n\tcc x\n  z");
  keys(&editor, ":set et!\n");
  TEST_CHECK(editor_uses_tabs(&editor));
  keys(&editor, "o\177\177\tw\033");
  check_string(text_of(&editor), "all:\n\tcc x\n  z\n\tw");
  TEST_CHECK(!editor.config.noexpandtab);  // the default is not touched
  editor_deinit(&editor);

  // a C file indented with tabs is in tab mode; :set sw works on spaces
  open_file(&editor, "t.c", "int f() {\n\treturn 0;\n}\n");
  TEST_CHECK(editor_uses_tabs(&editor));
  keys(&editor, "Go\tx\033");
  check_string(text_of(&editor), "int f() {\n\treturn 0;\n}\n\tx");
  keys(&editor, ":set noet\n:set sw=3\n:set et\n");
  editor_describe_indent(&editor, text, sizeof(text));
  check_string(text, "spaces:3");
  keys(&editor, "o\177\ty\033");
  check_string(text_of(&editor), "int f() {\n\treturn 0;\n}\n\tx\n   y");
  keys(&editor, ":set sw?\n");
  TEST_CHECK(editor.error_message != NULL &&
             strstr(editor.error_message, "shiftwidth=3") != NULL);
  TEST_CHECK(editor.config.shiftwidth == 0);
  editor_deinit(&editor);

  // the defaults from vi.conf: noexpandtab for files that do not say
  open_file(&editor, "notes.txt", "plain\n");
  editor.config.noexpandtab = true;
  editor.config.shiftwidth = 4;
  TEST_CHECK(editor_uses_tabs(&editor));
  editor.config.noexpandtab = false;
  editor_describe_indent(&editor, text, sizeof(text));
  check_string(text, "spaces:4");
  editor_deinit(&editor);
  remove_temp_dir();
}

// Ctrl-H is Backspace in insert mode and on the command line, as in vim
// (outside them it moves to the file tree).
void test_ctrl_h_is_backspace_when_typing(void) {
  make_temp_dir();
  Editor editor;
  open_file(&editor, "a.c", "abc\n");
  keys(&editor, "$a");
  editor_process_key(&editor, 8);
  check_string(text_of(&editor), "ab");
  editor_process_key(&editor, 27);
  keys(&editor, ":xy");
  editor_process_key(&editor, 8);
  check_string(editor.command.buffer, "x");
  editor_process_key(&editor, 27);
  editor_deinit(&editor);
  remove_temp_dir();
}

TEST_LIST = {
  {"ctrl_h_is_backspace_when_typing", test_ctrl_h_is_backspace_when_typing},
  {"makefile_inserts_tabs", test_makefile_inserts_tabs},
  {"set_tabstop_expandtab_shiftwidth", test_set_tabstop_expandtab_shiftwidth},
  {"indent_rules", test_indent_rules},
  {"indent_find_opener_and_unit", test_indent_find_opener_and_unit},
  {"enter_keeps_and_adds_indentation", test_enter_keeps_and_adds_indentation},
  {"enter_between_braces", test_enter_between_braces},
  {"escape_clears_unused_indentation", test_escape_clears_unused_indentation},
  {"o_and_O", test_o_and_O},
  {"tab_and_backspace_in_indentation", test_tab_and_backspace_in_indentation},
  {"backspace_joins_at_the_line_end", test_backspace_joins_at_the_line_end},
  {"noautoindent_and_plain_text", test_noautoindent_and_plain_text},
  {NULL, NULL},
};
