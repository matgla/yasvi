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
#include "autosave.h"

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
  void* make;        // struct MakeState*
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
void editor_autosave_tick(Editor* editor, long long now_ms);
void editor_close_current_buffer(Editor* editor);

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


#include <stdlib.h>
#include <sys/stat.h>

/* ---- helpers ---------------------------------------------------------- */

static char g_dir[64];

static void make_temp_dir(void) {
  snprintf(g_dir, sizeof(g_dir), "/tmp/yasvi_autosave_XXXXXX");
  TEST_ASSERT(mkdtemp(g_dir) != NULL);
  // Keep the cache and /tmp fallbacks inside the test's own directory.
  char cache[300];
  snprintf(cache, sizeof(cache), "%s/cache", g_dir);
  setenv("XDG_CACHE_HOME", cache, 1);
}

static void remove_temp_dir(void) {
  char cmd[300];
  snprintf(cmd, sizeof(cmd), "chmod -R u+w %s; rm -rf %s", g_dir, g_dir);
  TEST_CHECK(system(cmd) == 0);
  unsetenv("XDG_CACHE_HOME");
}

static char* path_in_dir(const char* name) {
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

// Whole file as a string (static); NULL if it does not exist.
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

static bool exists(const char* path) {
  return access(path, F_OK) == 0;
}

static void type_text(Editor* editor, const char* text) {
  editor_process_key(editor, 'i');
  for (const char* p = text; *p; ++p) {
    editor_process_key(editor, *p);
  }
  editor_process_key(editor, 27);
}

/* ---- paths ------------------------------------------------------------ */

void test_autosave_candidates_named(void) {
  setenv("XDG_CACHE_HOME", "/x", 1);
  char* c[AUTOSAVE_MAX_CANDIDATES];
  int n = autosave_candidates("/srv/src/main.c", c);
  TEST_CHECK(n == 3);
  TEST_CHECK(strcmp(c[0], "/srv/src/.main.c.yasvi-autosave") == 0);
  TEST_MSG("%s", c[0]);
  TEST_CHECK(strcmp(c[1], "/x/yasvi/%srv%src%main.c.yasvi-autosave") == 0);
  TEST_MSG("%s", c[1]);
  TEST_CHECK(strcmp(c[2], "/tmp/yasvi/%srv%src%main.c.yasvi-autosave") == 0);
  autosave_free_candidates(c, n);

  // A relative name sits beside itself and is made absolute for the cache.
  n = autosave_candidates("./notes", c);
  TEST_CHECK(strcmp(c[0], "./.notes.yasvi-autosave") == 0);
  char cwd[256];
  TEST_ASSERT(getcwd(cwd, sizeof(cwd)) != NULL);
  char want[400];
  snprintf(want, sizeof(want), "/x/yasvi/%s/notes.yasvi-autosave", cwd);
  for (char* p = want + strlen("/x/yasvi/"); *p; ++p) {
    if (*p == '/') {
      *p = '%';
    }
  }
  TEST_CHECK(strcmp(c[1], want) == 0);
  TEST_MSG("got %s want %s", c[1], want);
  autosave_free_candidates(c, n);
  unsetenv("XDG_CACHE_HOME");
}

void test_autosave_cache_dir_fallbacks(void) {
  char* old_home = getenv("HOME") ? strdup(getenv("HOME")) : NULL;
  unsetenv("XDG_CACHE_HOME");
  setenv("HOME", "/home/u", 1);
  char* dir = autosave_cache_dir();
  TEST_CHECK(strcmp(dir, "/home/u/.cache/yasvi") == 0);
  free(dir);
  unsetenv("HOME");
  dir = autosave_cache_dir();
  TEST_CHECK(strcmp(dir, "/root/.cache/yasvi") == 0);
  free(dir);
  if (old_home) {
    setenv("HOME", old_home, 1);
    free(old_home);
  }
}

void test_autosave_candidates_unnamed(void) {
  setenv("XDG_CACHE_HOME", "/x", 1);
  char* c[AUTOSAVE_MAX_CANDIDATES];
  int n = autosave_candidates(NULL, c);
  TEST_CHECK(n == 2);
  char want[64];
  snprintf(want, sizeof(want), "/x/yasvi/noname-%d.yasvi-autosave", (int)getpid());
  TEST_CHECK(strcmp(c[0], want) == 0);
  autosave_free_candidates(c, n);
  unsetenv("XDG_CACHE_HOME");
}

/* ---- writing ---------------------------------------------------------- */

void test_autosave_write_and_match(void) {
  make_temp_dir();
  Buffer* buffer = buffer_alloc();
  buffer_append_line(buffer, "one");
  buffer_append_line(buffer, "");
  buffer_append_line(buffer, "three");
  const char* path = path_in_dir("copy");
  TEST_CHECK(autosave_write_to(buffer, path));
  TEST_CHECK(strcmp(read_file(path), "one\n\nthree\n") == 0);
  TEST_CHECK(!exists(path_in_dir("copy.tmp")));
  TEST_CHECK(autosave_matches(buffer, path));
  buffer_row_replace_line(buffer->tail, "THREE");
  TEST_CHECK(!autosave_matches(buffer, path));
  buffer_append_line(buffer, "four");
  write_file(path, "one\n\nTHREE\n");
  TEST_CHECK(!autosave_matches(buffer, path));  // shorter file
  buffer_free(buffer);
  remove_temp_dir();
}

/* ---- editor: when it writes ------------------------------------------- */

void test_autosave_after_idle(void) {
  make_temp_dir();
  const char* file = path_in_dir("a.txt");
  const char* save = path_in_dir(".a.txt.yasvi-autosave");
  write_file(file, "hello\n");

  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, file);
  editor_autosave_tick(&editor, 0);
  TEST_CHECK(!exists(save));  // nothing edited

  type_text(&editor, "X");
  TEST_CHECK(buffer_is_modified(editor.current_buffer));
  editor_autosave_tick(&editor, 1000);  // first seen now
  editor_autosave_tick(&editor, 1000 + AUTOSAVE_IDLE_MS - 1);
  TEST_CHECK(!exists(save));
  editor_autosave_tick(&editor, 1000 + AUTOSAVE_IDLE_MS);
  TEST_CHECK(exists(save));
  TEST_CHECK(read_file(save) && strcmp(read_file(save), "Xhello\n") == 0);
  TEST_CHECK(strcmp(read_file(file), "hello\n") == 0);  // file untouched

  // Another edit restarts the idle timer.
  editor_process_key(&editor, '^');
  type_text(&editor, "Y");
  editor_autosave_tick(&editor, 5000);
  editor_autosave_tick(&editor, 5000 + AUTOSAVE_IDLE_MS - 1);
  TEST_CHECK(strcmp(read_file(save), "Xhello\n") == 0);
  editor_autosave_tick(&editor, 5000 + AUTOSAVE_IDLE_MS);
  TEST_CHECK(strcmp(read_file(save), "YXhello\n") == 0);

  // :w writes the file and drops the autosave.
  enter_command(&editor, "w");
  TEST_CHECK(!exists(save));
  TEST_CHECK(strcmp(read_file(file), "YXhello\n") == 0);
  TEST_CHECK(!buffer_is_modified(editor.current_buffer));
  editor_autosave_tick(&editor, 100000);
  TEST_CHECK(!exists(save));

  editor_deinit(&editor);
  remove_temp_dir();
}

void test_autosave_after_many_edits_without_pause(void) {
  make_temp_dir();
  const char* file = path_in_dir("b.txt");
  const char* save = path_in_dir(".b.txt.yasvi-autosave");
  write_file(file, "\n");

  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, file);
  editor_process_key(&editor, 'i');
  for (int i = 0; i < AUTOSAVE_MAX_PENDING - 1; ++i) {
    editor_process_key(&editor, 'a');
    editor_autosave_tick(&editor, 0);  // never idle
  }
  TEST_CHECK(!exists(save));
  editor_process_key(&editor, 'a');
  editor_autosave_tick(&editor, 0);
  TEST_CHECK(exists(save));
  editor_process_key(&editor, 27);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_autosave_read_only_dir_uses_cache(void) {
  if (geteuid() == 0) {
    return;  // root writes anywhere
  }
  make_temp_dir();
  char ro[300];
  snprintf(ro, sizeof(ro), "%s/ro", g_dir);
  TEST_ASSERT(mkdir(ro, 0755) == 0);
  char file[320];
  snprintf(file, sizeof(file), "%s/c.txt", ro);
  write_file(file, "text\n");
  chmod(ro, 0555);

  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, file);
  type_text(&editor, "Z");
  editor_autosave_tick(&editor, 0);
  editor_autosave_tick(&editor, AUTOSAVE_IDLE_MS);

  char beside[340];
  snprintf(beside, sizeof(beside), "%s/.c.txt.yasvi-autosave", ro);
  TEST_CHECK(!exists(beside));
  char want[600];
  snprintf(want, sizeof(want), "%s/cache/yasvi/%s.yasvi-autosave", g_dir, file);
  for (char* p = want + strlen(g_dir) + strlen("/cache/yasvi/"); *p; ++p) {
    if (*p == '/') {
      *p = '%';
    }
  }
  TEST_CHECK(exists(want));
  TEST_MSG("expected %s", want);
  TEST_CHECK(editor.current_buffer->autosave_path != NULL &&
             strcmp(editor.current_buffer->autosave_path, want) == 0);
  TEST_CHECK(read_file(want) && strcmp(read_file(want), "Ztext\n") == 0);

  editor_deinit(&editor);  // a clean exit deletes it
  TEST_CHECK(!exists(want));
  remove_temp_dir();
}

void test_autosave_deleted_on_exit_and_buffer_close(void) {
  make_temp_dir();
  const char* f1 = path_in_dir("one");
  const char* f2 = path_in_dir("two");
  const char* s1 = path_in_dir(".one.yasvi-autosave");
  const char* s2 = path_in_dir(".two.yasvi-autosave");
  write_file(f1, "1\n");
  write_file(f2, "2\n");

  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, f1);
  editor_load_file(&editor, f2);
  type_text(&editor, "a");          // buffer one
  enter_command(&editor, "bn");
  type_text(&editor, "b");          // buffer two
  editor_autosave_tick(&editor, 0);
  editor_autosave_tick(&editor, AUTOSAVE_IDLE_MS);
  TEST_CHECK(exists(s1) && exists(s2));

  enter_command(&editor, "bd");     // closes two
  TEST_CHECK(!exists(s2));
  TEST_CHECK(exists(s1));
  editor_deinit(&editor);
  TEST_CHECK(!exists(s1));
  remove_temp_dir();
}

void test_autosave_help_buffer_never_saved(void) {
  make_temp_dir();
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  enter_command(&editor, "help");
  TEST_CHECK(editor.current_buffer->autosave_disabled);
  type_text(&editor, "q");
  editor_autosave_tick(&editor, 0);
  editor_autosave_tick(&editor, AUTOSAVE_IDLE_MS);
  TEST_CHECK(editor.current_buffer->autosave_path == NULL);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_autosave_unnamed_buffer_named_by_write(void) {
  make_temp_dir();
  Editor editor;
  setup_editor(&editor);
  editor_create_new_file(&editor);
  type_text(&editor, "draft");
  editor_autosave_tick(&editor, 0);
  editor_autosave_tick(&editor, AUTOSAVE_IDLE_MS);
  char want[300];
  snprintf(want, sizeof(want), "%s/cache/yasvi/noname-%d.yasvi-autosave", g_dir,
           (int)getpid());
  TEST_CHECK(exists(want));
  TEST_CHECK(read_file(want) && strcmp(read_file(want), "draft\n") == 0);

  char cmd[300];
  snprintf(cmd, sizeof(cmd), "w %s", path_in_dir("named.txt"));
  enter_command(&editor, cmd);
  TEST_CHECK(!exists(want));
  TEST_CHECK(editor.current_buffer->filename != NULL &&
             strcmp(editor.current_buffer->filename, path_in_dir("named.txt")) == 0);
  TEST_CHECK(!buffer_is_modified(editor.current_buffer));
  editor_deinit(&editor);
  remove_temp_dir();
}

/* ---- editor: after a crash -------------------------------------------- */

void test_autosave_stale_same_as_file_is_dropped(void) {
  make_temp_dir();
  const char* file = path_in_dir("d.txt");
  const char* save = path_in_dir(".d.txt.yasvi-autosave");
  write_file(file, "same\n");
  write_file(save, "same\n");
  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, file);
  TEST_CHECK(editor.current_buffer->stale_autosave == NULL);
  TEST_CHECK(!exists(save));
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_autosave_recover(void) {
  make_temp_dir();
  const char* file = path_in_dir("e.c");
  const char* save = path_in_dir(".e.c.yasvi-autosave");
  write_file(file, "int a;\n");
  write_file(save, "int a;\nint lost_work;\n");

  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, file);
  Buffer* buffer = editor.current_buffer;
  TEST_CHECK(buffer->stale_autosave != NULL);
  TEST_CHECK(editor.error_message != NULL &&
             strstr(editor.error_message, ":recover") != NULL);
  TEST_CHECK(buffer->number_of_rows == 1);  // the file, not the autosave

  // Until decided, edits do not overwrite the crash's copy.
  type_text(&editor, "/");
  editor_autosave_tick(&editor, 0);
  editor_autosave_tick(&editor, AUTOSAVE_IDLE_MS);
  TEST_CHECK(strcmp(read_file(save), "int a;\nint lost_work;\n") == 0);

  enter_command(&editor, "recover");
  TEST_CHECK(buffer->stale_autosave == NULL);
  TEST_CHECK(buffer->number_of_rows == 2);
  TEST_CHECK(strcmp(buffer->head->data, "int a;") == 0);
  TEST_CHECK(strcmp(buffer->tail->data, "int lost_work;") == 0);
  TEST_CHECK(buffer->tail->filetype == buffer->filetype);
  TEST_CHECK(buffer_is_modified(buffer));
  TEST_CHECK(strcmp(read_file(file), "int a;\n") == 0);  // :w not yet
  TEST_CHECK(exists(save));  // still the copy until :w

  enter_command(&editor, "w");
  TEST_CHECK(strcmp(read_file(file), "int a;\nint lost_work;\n") == 0);
  TEST_CHECK(!exists(save));

  enter_command(&editor, "recover");  // nothing left
  TEST_CHECK(editor.error_message != NULL &&
             strstr(editor.error_message, "No autosave to recover") != NULL);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_autosave_delautosave(void) {
  make_temp_dir();
  const char* file = path_in_dir("f.txt");
  const char* save = path_in_dir(".f.txt.yasvi-autosave");
  write_file(file, "kept\n");
  write_file(save, "unwanted\n");

  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, file);
  TEST_CHECK(editor.current_buffer->stale_autosave != NULL);
  enter_command(&editor, "delautosave");
  TEST_CHECK(!exists(save));
  TEST_CHECK(editor.current_buffer->stale_autosave == NULL);
  TEST_CHECK(strcmp(editor.current_buffer->head->data, "kept") == 0);

  // Autosave works for the buffer again.
  type_text(&editor, "x");
  editor_autosave_tick(&editor, 0);
  editor_autosave_tick(&editor, AUTOSAVE_IDLE_MS);
  TEST_CHECK(read_file(save) && strcmp(read_file(save), "xkept\n") == 0);
  editor_deinit(&editor);
  remove_temp_dir();
}

void test_autosave_stale_in_cache_dir_found(void) {
  make_temp_dir();
  const char* file = path_in_dir("g.txt");
  write_file(file, "disk\n");
  char cachedir[300];
  snprintf(cachedir, sizeof(cachedir), "%s/cache/yasvi", g_dir);
  char cmd[400];
  snprintf(cmd, sizeof(cmd), "mkdir -p %s", cachedir);
  TEST_ASSERT(system(cmd) == 0);
  char save[1024];
  snprintf(save, sizeof(save), "%s/%s.yasvi-autosave", cachedir, file);
  for (char* p = save + strlen(cachedir) + 1; *p; ++p) {
    if (*p == '/') {
      *p = '%';
    }
  }
  write_file(save, "from cache\n");

  Editor editor;
  setup_editor(&editor);
  editor_load_file(&editor, file);
  TEST_CHECK(editor.current_buffer->stale_autosave != NULL &&
             strcmp(editor.current_buffer->stale_autosave, save) == 0);
  enter_command(&editor, "recover");
  TEST_CHECK(strcmp(editor.current_buffer->head->data, "from cache") == 0);
  editor_deinit(&editor);
  remove_temp_dir();
}

TEST_LIST = {
    {"autosave_candidates_named", test_autosave_candidates_named},
    {"autosave_cache_dir_fallbacks", test_autosave_cache_dir_fallbacks},
    {"autosave_candidates_unnamed", test_autosave_candidates_unnamed},
    {"autosave_write_and_match", test_autosave_write_and_match},
    {"autosave_after_idle", test_autosave_after_idle},
    {"autosave_after_many_edits_without_pause",
     test_autosave_after_many_edits_without_pause},
    {"autosave_read_only_dir_uses_cache", test_autosave_read_only_dir_uses_cache},
    {"autosave_deleted_on_exit_and_buffer_close",
     test_autosave_deleted_on_exit_and_buffer_close},
    {"autosave_help_buffer_never_saved", test_autosave_help_buffer_never_saved},
    {"autosave_unnamed_buffer_named_by_write",
     test_autosave_unnamed_buffer_named_by_write},
    {"autosave_stale_same_as_file_is_dropped",
     test_autosave_stale_same_as_file_is_dropped},
    {"autosave_recover", test_autosave_recover},
    {"autosave_delautosave", test_autosave_delautosave},
    {"autosave_stale_in_cache_dir_found", test_autosave_stale_in_cache_dir_found},
    {NULL, NULL}};
