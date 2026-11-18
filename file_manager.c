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

/* For strdup and strcasecmp */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "file_manager.h"

#include <dirent.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fileops.h"
#include "window.h"

#define FILE_MANAGER_DEFAULT_WIDTH 30
#define FILE_MANAGER_MIN_WIDTH 15
#define FILE_MANAGER_MAX_WIDTH 60

// Forward declarations
static void file_manager_entry_free(FileManagerEntry* entry);
// Forward declarations
static FileManagerEntry* file_manager_entry_create(const char* name,
                                                    const char* full_path,
                                                    FileManagerEntryType type,
                                                    int depth);
static void file_manager_scan_directory(FileManager* fm, FileManagerEntry* dir_entry);
static FileManagerEntry* file_manager_find_next_visible(FileManagerEntry* entry);
static FileManagerEntry* file_manager_find_prev_visible(FileManagerEntry* entry);
static void file_manager_sync_cursor(FileManager* fm);

FileManager* file_manager_alloc(void) {
  FileManager* fm = (FileManager*)malloc(sizeof(FileManager));
  if (fm) {
    memset(fm, 0, sizeof(FileManager));
    fm->width = FILE_MANAGER_DEFAULT_WIDTH;
  }
  return fm;
}

void file_manager_free(FileManager* fm) {
  if (!fm) {
    return;
  }
  if (fm->root) {
    file_manager_entry_free(fm->root);
  }
  free(fm->current_path);
  free(fm->prompt_target);
  free(fm);
}

static FileManagerEntry* file_manager_entry_create(const char* name,
                                                    const char* full_path,
                                                    FileManagerEntryType type,
                                                    int depth) {
  FileManagerEntry* entry = (FileManagerEntry*)malloc(sizeof(FileManagerEntry));
  if (!entry) {
    return NULL;
  }
  memset(entry, 0, sizeof(FileManagerEntry));

  entry->name = strdup(name);
  entry->full_path = strdup(full_path);
  entry->type = type;
  entry->depth = depth;
  entry->expanded = false;

  return entry;
}

static void file_manager_entry_free(FileManagerEntry* entry) {
  if (!entry) {
    return;
  }

  // Free all children first
  FileManagerEntry* child = entry->children;
  while (child) {
    FileManagerEntry* next = child->next;
    file_manager_entry_free(child);
    child = next;
  }

  free(entry->name);
  free(entry->full_path);
  free(entry);
}

static int compare_entries(const void* a, const void* b) {
  FileManagerEntry* ea = *(FileManagerEntry**)a;
  FileManagerEntry* eb = *(FileManagerEntry**)b;

  // Directories come first
  if (ea->type == FileManagerEntryType_Directory &&
      eb->type != FileManagerEntryType_Directory) {
    return -1;
  }
  if (ea->type != FileManagerEntryType_Directory &&
      eb->type == FileManagerEntryType_Directory) {
    return 1;
  }

  return strcasecmp(ea->name, eb->name);
}

static void file_manager_scan_directory(FileManager* fm, FileManagerEntry* dir_entry) {
  (void)fm;  // Unused for now, may be needed for caching in future
  if (!dir_entry || dir_entry->type != FileManagerEntryType_Directory) {
    return;
  }

  // Clear existing children
  FileManagerEntry* child = dir_entry->children;
  while (child) {
    FileManagerEntry* next = child->next;
    file_manager_entry_free(child);
    child = next;
  }
  dir_entry->children = NULL;
  dir_entry->last_child = NULL;

  DIR* dir = opendir(dir_entry->full_path);
  if (!dir) {
    return;
  }

  // Temporary array for sorting
  FileManagerEntry** temp_entries = NULL;
  int entry_count = 0;
  int entry_capacity = 64;

  temp_entries = malloc(sizeof(FileManagerEntry*) * entry_capacity);
  if (!temp_entries) {
    closedir(dir);
    return;
  }

  struct dirent* dent;
  while ((dent = readdir(dir)) != NULL) {
    // Skip hidden files and special directories
    if (dent->d_name[0] == '.') {
      if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
        continue;
      }
      // Optionally skip hidden files (starting with .)
      // continue; // Uncomment to hide hidden files
    }

    // Build full path
    char* full_path = path_join(dir_entry->full_path, dent->d_name);
    if (full_path == NULL) {
      continue;
    }

    // The type: a link to a directory opens like one.
    struct stat st;
    if (lstat(full_path, &st) != 0) {
      free(full_path);
      continue;
    }
    FileManagerEntryType type = FileManagerEntryType_File;
    if (S_ISDIR(st.st_mode)) {
      type = FileManagerEntryType_Directory;
    } else if (S_ISLNK(st.st_mode)) {
      struct stat target;
      type = stat(full_path, &target) == 0 && S_ISDIR(target.st_mode)
               ? FileManagerEntryType_Directory
               : FileManagerEntryType_Symlink;
    }

    if (entry_count >= entry_capacity) {
      entry_capacity *= 2;
      FileManagerEntry** new_temp = realloc(temp_entries, sizeof(FileManagerEntry*) * entry_capacity);
      if (!new_temp) {
        free(full_path);
        break;
      }
      temp_entries = new_temp;
    }

    temp_entries[entry_count] =
      file_manager_entry_create(dent->d_name, full_path, type, dir_entry->depth + 1);
    free(full_path);
    if (temp_entries[entry_count]) {
      temp_entries[entry_count]->parent = dir_entry;
      entry_count++;
    }
  }

  closedir(dir);

  // Sort entries
  if (entry_count > 0) {
    qsort(temp_entries, entry_count, sizeof(FileManagerEntry*), compare_entries);

    // Link children
    for (int i = 0; i < entry_count; i++) {
      if (i > 0) {
        temp_entries[i]->prev = temp_entries[i - 1];
      }
      if (i < entry_count - 1) {
        temp_entries[i]->next = temp_entries[i + 1];
      }
    }

    dir_entry->children = temp_entries[0];
    dir_entry->last_child = temp_entries[entry_count - 1];
  }

  free(temp_entries);
}

// ---- the expanded set: which directories are open, by path -----------------

typedef struct {
  char** paths;
  int count;
  int capacity;
} ExpandedSet;

static void expanded_collect(ExpandedSet* set, const FileManagerEntry* entry) {
  for (; entry != NULL; entry = entry->next) {
    if (entry->type != FileManagerEntryType_Directory || !entry->expanded) {
      continue;
    }
    if (set->count == set->capacity) {
      int capacity = set->capacity == 0 ? 16 : set->capacity * 2;
      char** grown = realloc(set->paths, sizeof(char*) * (size_t)capacity);
      if (grown == NULL) {
        return;
      }
      set->paths = grown;
      set->capacity = capacity;
    }
    set->paths[set->count] = strdup(entry->full_path);
    if (set->paths[set->count] != NULL) {
      set->count++;
    }
    expanded_collect(set, entry->children);
  }
}

static bool expanded_contains(const ExpandedSet* set, const char* path) {
  for (int i = 0; i < set->count; ++i) {
    if (strcmp(set->paths[i], path) == 0) {
      return true;
    }
  }
  return false;
}

static void expanded_free(ExpandedSet* set) {
  for (int i = 0; i < set->count; ++i) {
    free(set->paths[i]);
  }
  free(set->paths);
  set->paths = NULL;
  set->count = set->capacity = 0;
}

// Opens again every directory below `dir` that the set holds.
static void expanded_apply(FileManager* fm, FileManagerEntry* dir, const ExpandedSet* set) {
  for (FileManagerEntry* child = dir->children; child != NULL; child = child->next) {
    if (child->type == FileManagerEntryType_Directory &&
        expanded_contains(set, child->full_path)) {
      file_manager_scan_directory(fm, child);
      child->expanded = true;
      expanded_apply(fm, child, set);
    }
  }
}

// ---- cursor line and scrolling ------------------------------------------------

// The cursor's line counted from the root (line 0), so the highlight always
// matches the entry the keys act on; the view scrolls to keep it shown.
static void file_manager_sync_cursor(FileManager* fm) {
  if (fm->root == NULL) {
    return;
  }
  if (fm->cursor == NULL) {
    fm->cursor = fm->root;
  }
  int line = 0;
  FileManagerEntry* entry = fm->root;
  while (entry != NULL && entry != fm->cursor) {
    entry = file_manager_find_next_visible(entry);
    ++line;
  }
  if (entry == NULL) {
    // The cursor is in a collapsed directory: put it on the root.
    fm->cursor = fm->root;
    line = 0;
  }
  fm->cursor_line = line;
  if (fm->cursor_line < fm->scroll_offset) {
    fm->scroll_offset = fm->cursor_line;
  }
  if (fm->height > 0 && fm->cursor_line >= fm->scroll_offset + fm->height) {
    fm->scroll_offset = fm->cursor_line - fm->height + 1;
  }
  if (fm->scroll_offset < 0) {
    fm->scroll_offset = 0;
  }
}

// A new tree at `path`: directories expanded in the old tree that are also
// under the new root stay expanded.
static bool file_manager_set_root(FileManager* fm, const char* path) {
  char* absolute = path_absolute(path);
  if (absolute == NULL) {
    return false;
  }
  FileManagerEntry* root =
    file_manager_entry_create(".", absolute, FileManagerEntryType_Directory, 0);
  if (root == NULL) {
    free(absolute);
    return false;
  }
  ExpandedSet set = {0};
  if (fm->root != NULL) {
    expanded_collect(&set, fm->root);
    file_manager_entry_free(fm->root);
  }
  fm->root = root;
  root->expanded = true;
  file_manager_scan_directory(fm, root);
  expanded_apply(fm, root, &set);
  expanded_free(&set);
  free(fm->current_path);
  fm->current_path = absolute;
  fm->cursor = root;
  fm->cursor_line = 0;
  fm->scroll_offset = 0;
  return true;
}

bool file_manager_init(FileManager* fm, const char* path) {
  if (!fm) {
    return false;
  }
  char cwd[4096];
  const char* start_path = path;
  if (!start_path) {
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      return false;
    }
    start_path = cwd;
  }
  return file_manager_set_root(fm, start_path);
}

static FileManagerEntry* file_manager_find_next_visible(FileManagerEntry* entry) {
  if (!entry) {
    return NULL;
  }
  if (entry->type == FileManagerEntryType_Directory && entry->expanded &&
      entry->children) {
    return entry->children;
  }
  for (; entry != NULL; entry = entry->parent) {
    if (entry->next) {
      return entry->next;
    }
  }
  return NULL;
}

static FileManagerEntry* file_manager_find_prev_visible(FileManagerEntry* entry) {
  if (!entry) {
    return NULL;
  }
  if (entry->prev) {
    entry = entry->prev;
    while (entry->type == FileManagerEntryType_Directory && entry->expanded &&
           entry->last_child) {
      entry = entry->last_child;
    }
    return entry;
  }
  return entry->parent;
}

void file_manager_move_cursor_down(FileManager* fm) {
  if (!fm || !fm->cursor) {
    return;
  }
  FileManagerEntry* next = file_manager_find_next_visible(fm->cursor);
  if (next) {
    fm->cursor = next;
  }
  file_manager_sync_cursor(fm);
}

void file_manager_move_cursor_up(FileManager* fm) {
  if (!fm || !fm->cursor) {
    return;
  }
  FileManagerEntry* prev = file_manager_find_prev_visible(fm->cursor);
  if (prev) {
    fm->cursor = prev;
  }
  file_manager_sync_cursor(fm);
}

void file_manager_toggle_expand(FileManager* fm) {
  if (!fm || !fm->cursor) {
    return;
  }
  if (fm->cursor->type == FileManagerEntryType_Directory) {
    if (fm->cursor->expanded) {
      fm->cursor->expanded = false;
    } else {
      file_manager_scan_directory(fm, fm->cursor);
      fm->cursor->expanded = true;
    }
  }
  file_manager_sync_cursor(fm);
}

// The selected directory becomes the root.
void file_manager_enter_directory(FileManager* fm) {
  if (!fm || !fm->cursor || fm->cursor->type != FileManagerEntryType_Directory) {
    return;
  }
  char* path = strdup(fm->cursor->full_path);
  if (path == NULL) {
    return;
  }
  file_manager_set_root(fm, path);
  if (fm->root->children != NULL) {
    fm->cursor = fm->root->children;
  }
  file_manager_sync_cursor(fm);
  free(path);
}

// The root's parent becomes the root, with the old root expanded and selected.
void file_manager_go_to_parent(FileManager* fm) {
  if (!fm || !fm->root || strcmp(fm->root->full_path, "/") == 0) {
    return;
  }
  char* old_root = strdup(fm->root->full_path);
  char* parent = path_dirname(fm->root->full_path);
  if (old_root != NULL && parent != NULL && file_manager_set_root(fm, parent)) {
    file_manager_select_path(fm, old_root);
    if (fm->cursor != NULL && fm->cursor->type == FileManagerEntryType_Directory &&
        !fm->cursor->expanded) {
      file_manager_toggle_expand(fm);
    }
  }
  free(old_root);
  free(parent);
}

bool file_manager_select_path(FileManager* fm, const char* path) {
  if (!fm || !fm->root || !path) {
    return false;
  }
  char* absolute = path_absolute(path);
  if (absolute == NULL) {
    return false;
  }
  FileManagerEntry* entry = fm->root;
  bool found = false;
  while (entry != NULL) {
    if (strcmp(entry->full_path, absolute) == 0) {
      found = true;
      break;
    }
    if (entry->type != FileManagerEntryType_Directory ||
        !path_is_within(absolute, entry->full_path)) {
      break;
    }
    if (!entry->expanded) {
      file_manager_scan_directory(fm, entry);
      entry->expanded = true;
    }
    FileManagerEntry* child = entry->children;
    while (child != NULL && !path_is_within(absolute, child->full_path)) {
      child = child->next;
    }
    entry = child;
  }
  free(absolute);
  if (found) {
    fm->cursor = entry;
  }
  file_manager_sync_cursor(fm);
  return found;
}

void file_manager_refresh(FileManager* fm) {
  if (!fm || !fm->root) {
    return;
  }
  char* cursor_path = fm->cursor != NULL ? strdup(fm->cursor->full_path) : NULL;
  ExpandedSet set = {0};
  expanded_collect(&set, fm->root);
  file_manager_scan_directory(fm, fm->root);
  fm->root->expanded = true;
  expanded_apply(fm, fm->root, &set);
  expanded_free(&set);
  fm->cursor = fm->root;
  // The same path, else the nearest directory above it that is still there.
  while (cursor_path != NULL && !file_manager_select_path(fm, cursor_path)) {
    char* parent = path_dirname(cursor_path);
    bool at_top = parent == NULL || strcmp(parent, cursor_path) == 0 ||
                  !path_is_within(parent, fm->root->full_path);
    free(cursor_path);
    cursor_path = at_top ? NULL : parent;
    if (at_top) {
      free(parent);
    }
  }
  free(cursor_path);
  file_manager_sync_cursor(fm);
}

char* file_manager_target_directory(const FileManager* fm) {
  if (!fm || !fm->root) {
    return NULL;
  }
  const FileManagerEntry* entry = fm->cursor != NULL ? fm->cursor : fm->root;
  if (entry->type != FileManagerEntryType_Directory) {
    entry = entry->parent != NULL ? entry->parent : fm->root;
  }
  return strdup(entry->full_path);
}

void file_manager_prompt_start(FileManager* fm, FileManagerPrompt kind,
                               const char* target, const char* initial) {
  file_manager_prompt_end(fm);
  fm->prompt = kind;
  fm->prompt_target = target != NULL ? strdup(target) : NULL;
  fm->prompt_length = 0;
  if (initial != NULL) {
    fm->prompt_length = strlen(initial);
    if (fm->prompt_length > FILE_MANAGER_PROMPT_MAX) {
      fm->prompt_length = FILE_MANAGER_PROMPT_MAX;
    }
    memcpy(fm->prompt_input, initial, fm->prompt_length);
  }
  fm->prompt_input[fm->prompt_length] = '\0';
}

void file_manager_prompt_end(FileManager* fm) {
  fm->prompt = FileManagerPrompt_None;
  free(fm->prompt_target);
  fm->prompt_target = NULL;
  fm->prompt_length = 0;
  fm->prompt_input[0] = '\0';
}

// A path for the status line: relative to the root when under it.
static const char* file_manager_short_path(const FileManager* fm, const char* path) {
  size_t length = strlen(fm->root->full_path);
  if (strncmp(path, fm->root->full_path, length) == 0) {
    if (path[length] == '\0') {
      return ".";
    }
    if (path[length] == '/') {
      return path + length + 1;
    }
    if (length == 1) {
      return path + 1;  // the root is "/"
    }
  }
  return path;
}

void file_manager_prompt_label(const FileManager* fm, char* out, size_t size) {
  const char* target =
    fm->prompt_target != NULL ? file_manager_short_path(fm, fm->prompt_target) : "";
  switch (fm->prompt) {
    case FileManagerPrompt_NewFile:
      snprintf(out, size, "New file in %s/ (name/ = directory): ", target);
      break;
    case FileManagerPrompt_NewDirectory:
      snprintf(out, size, "New directory in %s/: ", target);
      break;
    case FileManagerPrompt_Rename:
      snprintf(out, size, "Rename %s to: ", target);
      break;
    case FileManagerPrompt_Copy:
      snprintf(out, size, "Copy %s to: ", target);
      break;
    case FileManagerPrompt_Delete: {
      const FileManagerEntry* entry = fm->cursor;
      bool dir = entry != NULL && entry->type == FileManagerEntryType_Directory;
      snprintf(out, size, dir ? "Delete %s/ and everything in it? (y/n) "
                              : "Delete %s? (y/n) ",
               target);
    } break;
    default:
      out[0] = '\0';
      break;
  }
}


void file_manager_toggle_visibility(FileManager* fm) {
  if (!fm) {
    return;
  }
  fm->visible = !fm->visible;
}

void file_manager_set_visible(FileManager* fm, bool visible) {
  if (!fm) {
    return;
  }
  fm->visible = visible;
}

bool file_manager_is_visible(const FileManager* fm) {
  return fm ? fm->visible : false;
}

void file_manager_set_width(FileManager* fm, int width) {
  if (!fm) {
    return;
  }
  if (width < FILE_MANAGER_MIN_WIDTH) {
    width = FILE_MANAGER_MIN_WIDTH;
  } else if (width > FILE_MANAGER_MAX_WIDTH) {
    width = FILE_MANAGER_MAX_WIDTH;
  }
  fm->width = width;
}

// One row: the root shows its path, the rest are indented under it with
// ASCII marks ("+ dir/" closed, "- dir/" open, "@ link"), so every byte is
// one column on any terminal.
static void file_manager_draw_row(const FileManagerEntry* entry, int row, int attributes,
                                  int width) {
  char display[512];
  if (entry->depth == 0) {
    const char* path = entry->full_path;
    int length = (int)strlen(path);
    if (length > width - 1 && width > 4) {
      snprintf(display, sizeof(display), "...%s", path + length - (width - 4));
    } else {
      snprintf(display, sizeof(display), "%s", path);
    }
  } else {
    const bool dir = entry->type == FileManagerEntryType_Directory;
    const char* mark = dir ? (entry->expanded ? "- " : "+ ")
                           : (entry->type == FileManagerEntryType_Symlink ? "@ " : "  ");
    int indent = (entry->depth - 1) * 2;
    if (indent > width / 2) {
      indent = width / 2;
    }
    snprintf(display, sizeof(display), "%*s%s%s%s", indent, "", mark, entry->name,
             dir ? "/" : "");
  }
  int length = (int)strlen(display);
  if (length > width) {
    // Cut long names with a '~' in the last column.
    display[width - 1] = '~';
    length = width;
  }
  while (length < width && length < (int)sizeof(display) - 1) {
    display[length++] = ' ';
  }
  display[length] = '\0';
  attrset(attributes);
  mvaddstr(row, 0, display);
}

void file_manager_draw(FileManager* fm, int start_row, int max_height) {
  if (!fm || !fm->visible || !fm->root) {
    return;
  }
  fm->height = max_height;
  file_manager_sync_cursor(fm);
  const int width = fm->width - 1;

  int line = 0;
  int row = 0;
  for (FileManagerEntry* entry = fm->root; entry != NULL && row < max_height;
       entry = file_manager_find_next_visible(entry), ++line) {
    if (line < fm->scroll_offset) {
      continue;
    }
    // The selection in reverse while the tree has the keys, bold otherwise.
    int attributes = entry != fm->cursor ? COLOR_OTHER
                     : fm->focused       ? (COLOR_PAIR(1) | A_REVERSE)
                                         : (COLOR_OTHER | A_BOLD);
    file_manager_draw_row(entry, start_row + row, attributes, width);
    ++row;
  }
  attrset(COLOR_OTHER);
  for (; row < max_height; ++row) {
    move(start_row + row, 0);
    for (int j = 0; j < width; j++) {
      addch(' ');
    }
  }
  attrset(COLOR_OTHER | A_DIM);
  for (int i = 0; i < max_height; i++) {
    mvaddch(start_row + i, fm->width - 1, ACS_VLINE);
  }
  attrset(COLOR_OTHER);
}


FileManagerEntry* file_manager_get_selected_entry(FileManager* fm) {
  return fm ? fm->cursor : NULL;
}

const char* file_manager_get_selected_path(FileManager* fm) {
  if (!fm || !fm->cursor) {
    return NULL;
  }
  return fm->cursor->full_path;
}

bool file_manager_selected_is_directory(const FileManager* fm) {
  if (!fm || !fm->cursor) {
    return false;
  }
  return fm->cursor->type == FileManagerEntryType_Directory;
}

void file_manager_rescan(FileManager* fm) {
  if (!fm || !fm->root) {
    return;
  }
  file_manager_scan_directory(fm, fm->root);
}

void file_manager_rescan_entry(FileManager* fm, FileManagerEntry* entry) {
  if (!fm || !entry) {
    return;
  }
  if (entry->type == FileManagerEntryType_Directory) {
    file_manager_scan_directory(fm, entry);
  }
}

const char* file_manager_get_current_path(const FileManager* fm) {
  return fm ? fm->current_path : NULL;
}
