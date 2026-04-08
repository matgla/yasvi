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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Minimal definitions needed for file_manager.h */
typedef enum {
  FileManagerEntryType_File,
  FileManagerEntryType_Directory,
  FileManagerEntryType_Symlink,
} FileManagerEntryType;

typedef struct FileManagerEntry {
  char* name;
  char* full_path;
  FileManagerEntryType type;
  bool expanded;
  int depth;
  struct FileManagerEntry* parent;
  struct FileManagerEntry* next;
  struct FileManagerEntry* prev;
  struct FileManagerEntry* children;
  struct FileManagerEntry* last_child;
} FileManagerEntry;

typedef struct FileManager {
  FileManagerEntry* root;
  FileManagerEntry* cursor;
  int cursor_line;
  int scroll_offset;
  bool visible;
  int width;
  int height;
  char* current_path;
} FileManager;

/* Forward declarations from file_manager.c that we test */
FileManager* file_manager_alloc(void);
void file_manager_free(FileManager* fm);
bool file_manager_init(FileManager* fm, const char* path);
void file_manager_move_cursor_down(FileManager* fm);
void file_manager_move_cursor_up(FileManager* fm);
void file_manager_toggle_expand(FileManager* fm);
void file_manager_toggle_visibility(FileManager* fm);
void file_manager_set_visible(FileManager* fm, bool visible);
bool file_manager_is_visible(const FileManager* fm);
void file_manager_go_to_parent(FileManager* fm);
void file_manager_enter_directory(FileManager* fm);
void file_manager_rescan(FileManager* fm);
void file_manager_rescan_entry(FileManager* fm, FileManagerEntry* entry);
void file_manager_set_width(FileManager* fm, int width);
FileManagerEntry* file_manager_get_selected_entry(FileManager* fm);
const char* file_manager_get_selected_path(FileManager* fm);
bool file_manager_selected_is_directory(const FileManager* fm);
const char* file_manager_get_current_path(const FileManager* fm);

/* Helper to create test directory structure */
static char test_dir[256];
static int test_dir_counter = 0;

static void setup_test_directory(void) {
  snprintf(test_dir, sizeof(test_dir), "/tmp/file_manager_test_%d_%d", getpid(), test_dir_counter++);
  mkdir(test_dir, 0755);
}

static void cleanup_test_directory(void) {
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
  system(cmd);
}

static void create_test_file(const char* filename) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", test_dir, filename);
  FILE* f = fopen(path, "w");
  if (f) {
    fprintf(f, "test content\n");
    fclose(f);
  }
}

static void create_test_subdir(const char* dirname) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", test_dir, dirname);
  mkdir(path, 0755);
}

/* Test file manager allocation */
void test_file_manager_alloc(void) {
  FileManager* fm = file_manager_alloc();
  TEST_CHECK(fm != NULL);
  TEST_CHECK(fm->root == NULL);
  TEST_CHECK(fm->cursor == NULL);
  TEST_CHECK(fm->visible == false);
  TEST_CHECK(fm->width == 30);  // Default width
  TEST_CHECK(fm->current_path == NULL);

  file_manager_free(fm);
}

/* Test file manager initialization */
void test_file_manager_init(void) {
  setup_test_directory();
  create_test_file("test1.txt");
  create_test_file("test2.c");
  create_test_subdir("subdir");

  FileManager* fm = file_manager_alloc();
  TEST_CHECK(file_manager_init(fm, test_dir) == true);

  TEST_CHECK(fm->root != NULL);
  TEST_CHECK(strcmp(fm->root->name, ".") == 0);
  TEST_CHECK(fm->root->type == FileManagerEntryType_Directory);
  TEST_CHECK(fm->root->expanded == true);
  TEST_CHECK(fm->cursor != NULL);
  TEST_CHECK(fm->current_path != NULL);
  TEST_CHECK(strcmp(fm->current_path, test_dir) == 0);

  file_manager_free(fm);
  cleanup_test_directory();
}

/* Test file manager visibility */
void test_file_manager_visibility(void) {
  FileManager* fm = file_manager_alloc();
  TEST_CHECK(file_manager_is_visible(fm) == false);

  file_manager_toggle_visibility(fm);
  TEST_CHECK(file_manager_is_visible(fm) == true);

  file_manager_toggle_visibility(fm);
  TEST_CHECK(file_manager_is_visible(fm) == false);

  file_manager_set_visible(fm, true);
  TEST_CHECK(file_manager_is_visible(fm) == true);

  file_manager_set_visible(fm, false);
  TEST_CHECK(file_manager_is_visible(fm) == false);

  file_manager_free(fm);
}

/* Test file manager width */
void test_file_manager_width(void) {
  FileManager* fm = file_manager_alloc();
  TEST_CHECK(fm->width == 30);

  file_manager_set_width(fm, 40);
  TEST_CHECK(fm->width == 40);

  file_manager_set_width(fm, 10);  // Below minimum
  TEST_CHECK(fm->width == 15);     // Should clamp to min

  file_manager_set_width(fm, 100);  // Above maximum
  TEST_CHECK(fm->width == 60);      // Should clamp to max

  file_manager_free(fm);
}

/* Test cursor movement down */
void test_file_manager_move_cursor_down(void) {
  setup_test_directory();
  create_test_file("aaa.txt");
  create_test_file("bbb.txt");
  create_test_file("ccc.txt");

  FileManager* fm = file_manager_alloc();
  file_manager_init(fm, test_dir);

  // Initial cursor should be at root
  TEST_CHECK(fm->cursor == fm->root);
  TEST_CHECK(fm->cursor_line == 0);

  // Move down to first child
  file_manager_move_cursor_down(fm);
  TEST_CHECK(fm->cursor != fm->root);
  TEST_CHECK(fm->cursor_line == 1);

  // Move down again
  FileManagerEntry* first = fm->cursor;
  file_manager_move_cursor_down(fm);
  TEST_CHECK(fm->cursor != first);
  TEST_CHECK(fm->cursor_line == 2);

  file_manager_free(fm);
  cleanup_test_directory();
}

/* Test cursor movement up */
void test_file_manager_move_cursor_up(void) {
  setup_test_directory();
  create_test_file("aaa.txt");
  create_test_file("bbb.txt");

  FileManager* fm = file_manager_alloc();
  file_manager_init(fm, test_dir);

  // Move down then up
  file_manager_move_cursor_down(fm);
  TEST_CHECK(fm->cursor_line == 1);

  file_manager_move_cursor_up(fm);
  TEST_CHECK(fm->cursor == fm->root);
  TEST_CHECK(fm->cursor_line == 0);

  // Try to move up from root (should stay at root)
  file_manager_move_cursor_up(fm);
  TEST_CHECK(fm->cursor == fm->root);
  TEST_CHECK(fm->cursor_line == 0);

  file_manager_free(fm);
  cleanup_test_directory();
}

/* Test directory expand/collapse */
void test_file_manager_toggle_expand(void) {
  setup_test_directory();
  create_test_subdir("subdir");
  create_test_file("file.txt");

  FileManager* fm = file_manager_alloc();
  file_manager_init(fm, test_dir);

  // Find the subdir entry
  FileManagerEntry* subdir = NULL;
  FileManagerEntry* child = fm->root->children;
  while (child) {
    if (child->type == FileManagerEntryType_Directory &&
        strcmp(child->name, "subdir") == 0) {
      subdir = child;
      break;
    }
    child = child->next;
  }
  TEST_CHECK(subdir != NULL);
  TEST_CHECK(subdir->expanded == false);

  // Set cursor to subdir and toggle expand
  fm->cursor = subdir;
  file_manager_toggle_expand(fm);
  TEST_CHECK(subdir->expanded == true);

  // Toggle again to collapse
  file_manager_toggle_expand(fm);
  TEST_CHECK(subdir->expanded == false);

  file_manager_free(fm);
  cleanup_test_directory();
}

/* Test entry selection */
void test_file_manager_get_selected(void) {
  setup_test_directory();
  create_test_file("target.txt");

  FileManager* fm = file_manager_alloc();
  file_manager_init(fm, test_dir);

  // Move to first child (should be our file)
  file_manager_move_cursor_down(fm);

  FileManagerEntry* selected = file_manager_get_selected_entry(fm);
  TEST_CHECK(selected != NULL);
  TEST_CHECK(strcmp(selected->name, "target.txt") == 0);

  const char* path = file_manager_get_selected_path(fm);
  TEST_CHECK(path != NULL);
  TEST_CHECK(strstr(path, "target.txt") != NULL);

  TEST_CHECK(file_manager_selected_is_directory(fm) == false);

  file_manager_free(fm);
  cleanup_test_directory();
}

/* Test empty directory handling */
void test_file_manager_empty_directory(void) {
  setup_test_directory();

  FileManager* fm = file_manager_alloc();
  file_manager_init(fm, test_dir);

  // Root should exist and be expanded
  TEST_CHECK(fm->root != NULL);
  TEST_CHECK(fm->root->children == NULL);  // No children

  // Cursor should be at root
  TEST_CHECK(fm->cursor == fm->root);

  // Move down should have no effect
  file_manager_move_cursor_down(fm);
  TEST_CHECK(fm->cursor == fm->root);

  file_manager_free(fm);
  cleanup_test_directory();
}

/* Test get current path */
void test_file_manager_get_current_path(void) {
  setup_test_directory();

  FileManager* fm = file_manager_alloc();
  file_manager_init(fm, test_dir);

  const char* path = file_manager_get_current_path(fm);
  TEST_CHECK(path != NULL);
  TEST_CHECK(strcmp(path, test_dir) == 0);

  file_manager_free(fm);
  cleanup_test_directory();
}

/* Test NULL handling */
void test_file_manager_null_handling(void) {
  // All these should not crash
  file_manager_free(NULL);
  file_manager_toggle_visibility(NULL);
  file_manager_set_visible(NULL, true);
  TEST_CHECK(file_manager_is_visible(NULL) == false);
  file_manager_set_width(NULL, 40);
  file_manager_move_cursor_down(NULL);
  file_manager_move_cursor_up(NULL);
  file_manager_toggle_expand(NULL);
  TEST_CHECK(file_manager_get_selected_entry(NULL) == NULL);
  TEST_CHECK(file_manager_get_selected_path(NULL) == NULL);
  TEST_CHECK(file_manager_selected_is_directory(NULL) == false);
  TEST_CHECK(file_manager_get_current_path(NULL) == NULL);
}

TEST_LIST = {
    {"test_file_manager_alloc", test_file_manager_alloc},
    {"test_file_manager_init", test_file_manager_init},
    {"test_file_manager_visibility", test_file_manager_visibility},
    {"test_file_manager_width", test_file_manager_width},
    {"test_file_manager_move_cursor_down", test_file_manager_move_cursor_down},
    {"test_file_manager_move_cursor_up", test_file_manager_move_cursor_up},
    {"test_file_manager_toggle_expand", test_file_manager_toggle_expand},
    {"test_file_manager_get_selected", test_file_manager_get_selected},
    {"test_file_manager_empty_directory", test_file_manager_empty_directory},
    {"test_file_manager_get_current_path", test_file_manager_get_current_path},
    {"test_file_manager_null_handling", test_file_manager_null_handling},

    {NULL, NULL}
};
