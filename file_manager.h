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

#define FILE_MANAGER_PROMPT_MAX 255

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

  // A question on the command line while the tree has the keys (new name,
  // rename, copy, delete). editor.c asks it and acts on the answer.
  int prompt;                  // FileManagerPrompt
  char prompt_input[FILE_MANAGER_PROMPT_MAX + 1];
  size_t prompt_length;
  char* prompt_target;         // the path the question is about (malloc'd)
  bool focused;                // the tree has the keys (editor.c sets it)
} FileManager;

typedef enum {
  FileManagerPrompt_None,
  FileManagerPrompt_NewFile,       // a: name in the directory, "dir/" makes one
  FileManagerPrompt_NewDirectory,  // A
  FileManagerPrompt_Rename,        // r: new name or path, from the entry's directory
  FileManagerPrompt_Copy,          // c: likewise
  FileManagerPrompt_Delete,        // d: y deletes
} FileManagerPrompt;

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

// Reads every expanded directory again (files made or deleted elsewhere
// show up). Expanded directories stay expanded and the cursor stays on the
// same path, or on the nearest entry above where it was.
void file_manager_refresh(FileManager* fm);

// Puts the cursor on `path`, expanding the directories above it; false when
// it is not under the root or not there.
bool file_manager_select_path(FileManager* fm, const char* path);

// Where a new entry goes: the selected directory, or the directory of the
// selected file (malloc'd).
char* file_manager_target_directory(const FileManager* fm);

// The prompt: start one about `target` with `initial` typed, end it.
void file_manager_prompt_start(FileManager* fm, FileManagerPrompt kind,
                               const char* target, const char* initial);
void file_manager_prompt_end(FileManager* fm);
// The question shown before what is typed ("New file in src/: ").
void file_manager_prompt_label(const FileManager* fm, char* out, size_t size);
