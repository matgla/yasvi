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
static void file_manager_draw_entry(FileManagerEntry* entry,
                                     int* line,
                                     int cursor_line,
                                     int start_row,
                                     int max_height,
                                     int width);
static FileManagerEntry* file_manager_find_next_visible(FileManagerEntry* entry);
static FileManagerEntry* file_manager_find_prev_visible(FileManagerEntry* entry);
static FileManagerEntry* file_manager_find_first_visible(FileManagerEntry* root);

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
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_entry->full_path,
             dent->d_name);

    // Get file type
    struct stat st;
    if (stat(full_path, &st) != 0) {
      continue;
    }

    FileManagerEntryType type = FileManagerEntryType_File;
    if (S_ISDIR(st.st_mode)) {
      type = FileManagerEntryType_Directory;
    } else if (S_ISLNK(st.st_mode)) {
      type = FileManagerEntryType_Symlink;
    }

    if (entry_count >= entry_capacity) {
      entry_capacity *= 2;
      FileManagerEntry** new_temp = realloc(temp_entries, sizeof(FileManagerEntry*) * entry_capacity);
      if (!new_temp) {
        break;
      }
      temp_entries = new_temp;
    }

    temp_entries[entry_count] =
      file_manager_entry_create(dent->d_name, full_path, type, dir_entry->depth + 1);
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

bool file_manager_init(FileManager* fm, const char* path) {
  if (!fm) {
    return false;
  }

  // Get current working directory if no path provided
  char cwd[4096];
  const char* start_path = path;
  if (!start_path) {
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      return false;
    }
    start_path = cwd;
  }

  fm->current_path = strdup(start_path);

  // Create root entry
  fm->root = file_manager_entry_create(".", start_path, FileManagerEntryType_Directory, 0);
  if (!fm->root) {
    return false;
  }

  fm->root->expanded = true;
  file_manager_scan_directory(fm, fm->root);

  // Set cursor to first entry
  fm->cursor = file_manager_find_first_visible(fm->root);
  fm->cursor_line = 0;
  fm->scroll_offset = 0;

  return true;
}

static FileManagerEntry* file_manager_find_first_visible(FileManagerEntry* root) {
  if (!root) {
    return NULL;
  }
  return root;
}

static FileManagerEntry* file_manager_find_next_visible(FileManagerEntry* entry) {
  if (!entry) {
    return NULL;
  }

  // If directory is expanded, go to first child
  if (entry->type == FileManagerEntryType_Directory && entry->expanded &&
      entry->children) {
    return entry->children;
  }

  // Otherwise try next sibling
  if (entry->next) {
    return entry->next;
  }

  // Go up until we find a parent with a next sibling
  FileManagerEntry* parent = entry->parent;
  while (parent) {
    if (parent->next) {
      return parent->next;
    }
    parent = parent->parent;
  }

  return NULL;
}

static FileManagerEntry* file_manager_find_prev_visible(FileManagerEntry* entry) {
  if (!entry) {
    return NULL;
  }

  // Try previous sibling first
  if (entry->prev) {
    entry = entry->prev;
    // Then go to last visible child of this sibling
    while (entry->type == FileManagerEntryType_Directory && entry->expanded &&
           entry->last_child) {
      entry = entry->last_child;
    }
    return entry;
  }

  // Return parent if no previous sibling
  return entry->parent;
}

void file_manager_move_cursor_down(FileManager* fm) {
  if (!fm || !fm->cursor) {
    return;
  }

  FileManagerEntry* next = file_manager_find_next_visible(fm->cursor);
  if (next) {
    fm->cursor = next;
    fm->cursor_line++;

    // Adjust scroll if cursor goes below visible area
    if (fm->cursor_line >= fm->scroll_offset + fm->height) {
      fm->scroll_offset++;
    }
  }
}

void file_manager_move_cursor_up(FileManager* fm) {
  if (!fm || !fm->cursor) {
    return;
  }

  FileManagerEntry* prev = file_manager_find_prev_visible(fm->cursor);
  if (prev) {
    fm->cursor = prev;
    fm->cursor_line--;

    // Adjust scroll if cursor goes above visible area
    if (fm->cursor_line < fm->scroll_offset) {
      fm->scroll_offset = fm->cursor_line;
    }
  }
}

void file_manager_toggle_expand(FileManager* fm) {
  if (!fm || !fm->cursor) {
    return;
  }

  if (fm->cursor->type == FileManagerEntryType_Directory) {
    if (fm->cursor->expanded) {
      fm->cursor->expanded = false;
    } else {
      // Scan and expand
      file_manager_scan_directory(fm, fm->cursor);
      fm->cursor->expanded = true;
    }
  }
}

void file_manager_enter_directory(FileManager* fm) {
  if (!fm || !fm->cursor) {
    return;
  }

  if (fm->cursor->type == FileManagerEntryType_Directory) {
    // Rescan current directory
    file_manager_scan_directory(fm, fm->cursor);

    // Change current path to this directory
    free(fm->current_path);
    fm->current_path = strdup(fm->cursor->full_path);

    // Collapse current root
    fm->root->expanded = false;

    // Make this entry the new root
    FileManagerEntry* old_root = fm->root;

    // Detach from parent
    if (fm->cursor->parent) {
      if (fm->cursor->prev) {
        fm->cursor->prev->next = fm->cursor->next;
      } else {
        fm->cursor->parent->children = fm->cursor->next;
      }
      if (fm->cursor->next) {
        fm->cursor->next->prev = fm->cursor->prev;
      } else {
        fm->cursor->parent->last_child = fm->cursor->prev;
      }
    }

    fm->root = fm->cursor;
    fm->root->parent = NULL;
    fm->root->prev = NULL;
    fm->root->next = NULL;
    fm->root->depth = 0;
    fm->root->expanded = true;

    // Rescan the new root
    file_manager_scan_directory(fm, fm->root);

    // Set cursor to first child or root
    fm->cursor = fm->root->children ? fm->root->children : fm->root;
    fm->cursor_line = fm->cursor == fm->root ? 0 : 1;
    fm->scroll_offset = 0;

    // Clean up old root
    file_manager_entry_free(old_root);
  }
}

void file_manager_go_to_parent(FileManager* fm) {
  if (!fm || !fm->root) {
    return;
  }

  // Get parent directory path
  char parent_path[4096];
  strncpy(parent_path, fm->root->full_path, sizeof(parent_path) - 1);
  parent_path[sizeof(parent_path) - 1] = '\0';

  char* last_slash = strrchr(parent_path, '/');
  if (!last_slash || last_slash == parent_path) {
    // Already at root or can't go up
    return;
  }

  *last_slash = '\0';
  if (strlen(parent_path) == 0) {
    strcpy(parent_path, "/");
  }

  // Create new root for parent
  FileManagerEntry* old_root = fm->root;
  const char* dir_name = last_slash + 1;
  if (*dir_name == '\0') {
    dir_name = ".";
  }

  fm->root = file_manager_entry_create(dir_name, old_root->full_path,
                                        FileManagerEntryType_Directory, 0);
  if (!fm->root) {
    fm->root = old_root;
    return;
  }

  fm->current_path = strdup(parent_path);
  fm->root->expanded = true;
  file_manager_scan_directory(fm, fm->root);

  // Find and expand the entry that corresponds to old directory
  FileManagerEntry* child = fm->root->children;
  while (child) {
    if (strcmp(child->name, dir_name) == 0) {
      child->expanded = true;
      file_manager_scan_directory(fm, child);
      fm->cursor = child;
      break;
    }
    child = child->next;
  }

  if (!child) {
    fm->cursor = fm->root->children ? fm->root->children : fm->root;
  }

  fm->cursor_line = 0;
  fm->scroll_offset = 0;
  file_manager_entry_free(old_root);
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

static void file_manager_draw_entry(FileManagerEntry* entry,
                                     int* line,
                                     int cursor_line,
                                     int start_row,
                                     int max_height,
                                     int width) {
  if (!entry) {
    return;
  }

  // Skip if before scroll offset
  if (*line < 0) {
    (*line)++;
    if (entry->type == FileManagerEntryType_Directory && entry->expanded) {
      FileManagerEntry* child = entry->children;
      while (child) {
        file_manager_draw_entry(child, line, cursor_line, start_row, max_height, width);
        child = child->next;
      }
    }
    return;
  }

  // Stop if beyond visible area
  if (*line >= max_height) {
    (*line)++;
    return;
  }

  int row = start_row + *line;

  // Draw cursor indicator
  if (*line == cursor_line) {
    attrset(COLOR_PAIR(1) | A_REVERSE);
  } else {
    attrset(COLOR_OTHER);
  }

  // Build display string
  char display[256];
  int indent = entry->depth * 2;
  const char* icon = "";

  if (entry->type == FileManagerEntryType_Directory) {
    icon = entry->expanded ? "▾ " : "▸ ";
  } else if (entry->type == FileManagerEntryType_Symlink) {
    icon = "@ ";
  } else {
    icon = "  ";
  }

  // Calculate available space for name
  int name_width = width - indent - 2 - 1; // indent + icon + margin
  if (name_width < 1) {
    name_width = 1;
  }

  // Truncate name if too long
  char truncated_name[128];
  if ((int)strlen(entry->name) > name_width) {
    strncpy(truncated_name, entry->name, name_width - 3);
    truncated_name[name_width - 3] = '\0';
    strcat(truncated_name, "...");
  } else {
    strncpy(truncated_name, entry->name, sizeof(truncated_name) - 1);
    truncated_name[sizeof(truncated_name) - 1] = '\0';
  }

  snprintf(display, sizeof(display), "%*s%s%s", indent, "", icon, truncated_name);

  // Pad to width
  int display_len = strlen(display);
  if (display_len < width) {
    for (int i = display_len; i < width; i++) {
      strcat(display, " ");
    }
  }

  // Truncate to width
  display[width] = '\0';

  mvaddstr(row, 0, display);

  (*line)++;

  // Draw children if expanded
  if (entry->type == FileManagerEntryType_Directory && entry->expanded) {
    FileManagerEntry* child = entry->children;
    while (child) {
      file_manager_draw_entry(child, line, cursor_line, start_row, max_height, width);
      child = child->next;
    }
  }
}

void file_manager_draw(FileManager* fm, int start_row, int max_height) {
  if (!fm || !fm->visible || !fm->root) {
    return;
  }

  fm->height = max_height;

  // Clear the sidebar area
  attrset(COLOR_OTHER);
  for (int i = 0; i < max_height; i++) {
    move(start_row + i, 0);
    for (int j = 0; j < fm->width; j++) {
      addch(' ');
    }
  }

  // Draw vertical separator
  attrset(COLOR_OTHER | A_DIM);
  for (int i = 0; i < max_height; i++) {
    mvaddch(start_row + i, fm->width - 1, ACS_VLINE);
  }
  attrset(COLOR_OTHER);

  // Draw entries
  int line = -fm->scroll_offset;
  file_manager_draw_entry(fm->root, &line, fm->cursor_line - fm->scroll_offset,
                          start_row, max_height, fm->width - 1);

  // Clear remaining lines
  attrset(COLOR_OTHER);
  for (int i = line; i < max_height && i >= 0; i++) {
    move(start_row + i, 0);
    for (int j = 0; j < fm->width - 1; j++) {
      addch(' ');
    }
  }
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
