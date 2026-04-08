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

#pragma once

#include <stdbool.h>
#include <stddef.h>

// Entry types in the file manager
typedef enum {
  FileManagerEntryType_File,
  FileManagerEntryType_Directory,
  FileManagerEntryType_Symlink,
} FileManagerEntryType;

// A single entry in the file manager (file or directory)
typedef struct FileManagerEntry {
  char* name;
  char* full_path;
  FileManagerEntryType type;
  bool expanded;           // For directories: are children visible?
  int depth;               // Nesting level for tree display
  struct FileManagerEntry* parent;
  struct FileManagerEntry* next;     // Sibling
  struct FileManagerEntry* prev;     // Sibling
  struct FileManagerEntry* children; // For directories
  struct FileManagerEntry* last_child;
} FileManagerEntry;

// File manager state
typedef struct FileManager {
  FileManagerEntry* root;      // Root entry (current directory)
  FileManagerEntry* cursor;    // Currently selected entry
  int cursor_line;             // Line number for display
  int scroll_offset;           // Scroll position for long lists
  bool visible;                // Is sidebar visible?
  int width;                   // Sidebar width in characters
  int height;                  // Visible height
  char* current_path;          // Current working directory path
} FileManager;

// Lifecycle
FileManager* file_manager_alloc(void);
void file_manager_free(FileManager* fm);
bool file_manager_init(FileManager* fm, const char* path);

// Navigation
void file_manager_move_cursor_down(FileManager* fm);
void file_manager_move_cursor_up(FileManager* fm);
void file_manager_toggle_expand(FileManager* fm);
void file_manager_enter_directory(FileManager* fm);
void file_manager_go_to_parent(FileManager* fm);

// Visibility
void file_manager_toggle_visibility(FileManager* fm);
void file_manager_set_visible(FileManager* fm, bool visible);
bool file_manager_is_visible(const FileManager* fm);

// Drawing
void file_manager_draw(FileManager* fm, int start_row, int max_height);
void file_manager_set_width(FileManager* fm, int width);

// Entry access
FileManagerEntry* file_manager_get_selected_entry(FileManager* fm);
const char* file_manager_get_selected_path(FileManager* fm);
bool file_manager_selected_is_directory(const FileManager* fm);

// Refresh/rescan
void file_manager_rescan(FileManager* fm);
void file_manager_rescan_entry(FileManager* fm, FileManagerEntry* entry);

// Utility
const char* file_manager_get_current_path(const FileManager* fm);
