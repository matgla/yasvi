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

#define _GNU_SOURCE  // getline, ssize_t

#include "editor.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ncurses.h>

#include "autosave.h"
#include "command.h"
#include "completion.h"
#include "config.h"
#include "help.h"
#include "highlight.h"
#include "file_manager.h"
#include "fileops.h"
#include "make.h"
#include "man.h"
#include "record.h"
#include "indent.h"
#include "run.h"
#include "search.h"
#include "tags.h"
#include "termpanel.h"
#include "toolbar.h"
#include "toolbar_widgets.h"

#define EDITOR_TOP_BAR_HEIGHT 2
// 1 is for tab bar
// 1 is for separator line
// 1 is for command line
// 1 is for status bar
#define EDITOR_TOOLBAR_HEIGHT 3
#define EDITOR_KEY_CTRL_C 0x03  // :term: interrupt what the shell runs
#define EDITOR_KEY_CTRL_D 0x04  // :term: end the shell's input
#define EDITOR_KEY_CTRL_K 0x0b  // window up: from the :term panel to the text
#define EDITOR_KEY_CTRL_U 0x15  // :term: clear the typed line
#define EDITOR_KEY_CTRL_W 0x17  // window commands: Ctrl-W j/k/w/c

// Insert-mode completion keys (raw mode delivers the control codes).
#define EDITOR_KEY_CTRL_E 0x05  // cancel: put back the typed word
#define EDITOR_KEY_CTRL_N 0x0e  // next candidate
#define EDITOR_KEY_CTRL_P 0x10  // previous candidate
#define EDITOR_KEY_CTRL_Y 0x19  // accept
#define EDITOR_KEY_CTRL_H 0x08  // window left: the file manager
#define EDITOR_KEY_CTRL_L 0x0c  // window right: the editor
#define EDITOR_COMPLETION_MENU_ROWS 8

typedef enum {
  CommandResult_Success = 0,
  CommandResult_ShouldExit = 2,
  // The command failed and the handler has already reported why via
  // editor_set_error_message ("Failed to open file for writing", "Invalid
  // substitute syntax", "Command not found", ...). Every failing path owns its
  // own message, so the caller never has to guess one -- guessing is how a
  // failed ':w' used to be reported as an unknown command.
  CommandResult_Error = 3,
} CommandResult;

static void editor_move_cursor_to_end(Editor* editor);
static void editor_move_cursor_x(Editor* editor, int x, bool insert_mode);
static void editor_move_cursor_y(Editor* editor, int y);
static bool editor_process_key_sequence(Editor* editor, int key);
static void editor_set_error_message(Editor* editor, const char* message);
static void editor_set_status_message(Editor* editor, const char* message);
static void editor_home_cursor_x(Editor* editor);
static void editor_home_cursor_y(Editor* editor);
static void editor_home_cursor_xy(Editor* editor);
static void editor_fix_cursor_position(Editor* editor);
static int editor_get_content_offset_x(const Editor* editor);
static char* editor_word_under_cursor(Editor* editor);
static void editor_goto_position(Editor* editor, int line, int column);
static void editor_complete(Editor* editor, int step);
static void editor_completion_end(Editor* editor, bool restore);
static void editor_draw_completion_menu(Editor* editor);
static bool editor_append_buffer(Editor* editor, Buffer* buffer);
static bool editor_process_build_command(Editor* editor, CommandResult* result);
static void editor_man_under_cursor(Editor* editor);
static void editor_make_state_free(Editor* editor);
static char* editor_expand_command(Editor* editor, const char* command);
static bool editor_process_open_command(Editor* editor, CommandResult* result);
static void editor_complete_command_path(Editor* editor);
static void editor_file_manager_prompt_key(Editor* editor, int key);
static void editor_file_manager_start_prompt(Editor* editor, FileManagerPrompt kind);
static void editor_draw_file_manager_prompt(Editor* editor);
static void editor_clear_error_message(Editor* editor);
static void editor_set_cursor_column(Editor* editor, int column);
static void editor_insert_newline(Editor* editor);
static void editor_open_line_above(Editor* editor);

// Rows under the text: the :term panel when shown, then the toolbar.
static int editor_bottom_height(const Editor* editor) {
  const TermPanel* panel = editor->terminal;
  return EDITOR_TOOLBAR_HEIGHT + (panel != NULL && panel->visible ? panel->rows : 0);
}

#ifdef __GNUC__
char* itoa(int n, char* s, int base) {
  char* p = s;
  int sign = n < 0 ? -1 : 1;
  if (sign < 0)
    n = -n;
  do {
    *p++ = "0123456789abcdef"[n % base];
    n /= base;
  } while (n);
  if (sign < 0)
    *p++ = '-';
  *p-- = '\0';
  while (s < p) {
    char tmp = *s;
    *s++ = *p;
    *p-- = tmp;
  }
  return s;
}
#endif

int editor_get_cursor_x(const Editor* editor) {
  int content_offset = editor_get_content_offset_x(editor);
  int line_digit_width = 4;  // Base line number width
  if (editor->cursor.x + editor->start_column < content_offset + line_digit_width) {
    return 0;
  }
  return editor->cursor.x + editor->start_column - content_offset - line_digit_width;
}

static void editor_home_cursor_x(Editor* editor) {
  editor->cursor.x = editor->number_of_line_digits;
  editor->start_column = 0;
}

// Every edit of the current buffer's text goes through here (autosave, [+]).
static void editor_note_change(Editor* editor) {
  buffer_note_change(editor->current_buffer);
}

static void editor_mark_dirty_whole_screen(Editor* editor) {
  const int number_of_lines =
    editor->window.height - EDITOR_TOP_BAR_HEIGHT - editor_bottom_height(editor);
  BufferRow* row = buffer_get_row(editor->current_buffer, editor->start_line);
  if (row == NULL) {
    return;  // No rows in the buffer
  }
  for (int y = 0; y < number_of_lines; ++y) {
    buffer_row_mark_dirty(row);
    if (row->next != NULL) {
      row = row->next;
    } else {
      break;  // No more rows to mark
    }
  }
}

static void editor_mark_dirty_from_cursor(Editor* editor) {
  BufferRow* row = buffer_get_row(editor->current_buffer, editor->start_line);
  if (row == NULL) {
    return;  // No rows in the buffer
  }
  for (int y = 0; y < editor->window.height; ++y) {
    if (row != NULL) {
      buffer_row_mark_dirty(row);
      row = row->next;
    } else {
      break;  // No more rows to mark
    }
  }
}

static void editor_home_cursor_y(Editor* editor) {
  editor->cursor.y = EDITOR_TOP_BAR_HEIGHT;
  editor->start_line = 0;
  editor_mark_dirty_whole_screen(editor);
}

static void editor_home_cursor_xy(Editor* editor) {
  editor_home_cursor_x(editor);
  editor_home_cursor_y(editor);
}

// Only printable ASCII can be part of a ':' command or a search pattern. The
// terminal hands us more than the user typed -- a stray NUL or control byte
// around Enter (miniterm does this), and KEY_* codes such as KEY_UP are > 255
// and become junk once cast to char. Any of those silently lands in the buffer
// and turns ":w" into an unknown command, so drop everything else here.
static bool editor_is_printable_key(int key) {
  return key >= 0x20 && key <= 0x7E;
}

// true if editor loop should continue
static bool editor_collect_command(Editor* editor, int key) {
  if (key == '\n') {
    editor->state = EditorState_ProcessingCommand;
    return true;
  } else if (key == 27) {
    // cancel command
    command_deinit(&editor->command);
    editor->state = EditorState_Running;
    return true;
  } else if (key == KEY_BACKSPACE || key == 127 || key == EDITOR_KEY_CTRL_H) {
    if (editor->command.cursor_position > 0) {
      editor->command.cursor_position--;
      editor->command.buffer[editor->command.cursor_position] = '\0';
    }
    return false;
  } else if (key == '\t') {
    editor_complete_command_path(editor);
    return false;
  }
  if (!editor_is_printable_key(key)) {
    return false;  // terminal noise, not part of the command
  }
  command_append(&editor->command, (char)key);
  return false;
}

static void editor_clear_error_message(Editor* editor) {
  if (editor->error_message) {
    for (size_t i = 0; i < strlen(editor->error_message); ++i) {
      mvaddch(editor->window.height - 1, 1 + i, ' ');
    }
    free(editor->error_message);
    editor->error_message = NULL;
  }
}

// Handle search pattern input (for / and ? commands)
// Returns true if search should continue to next state
// Puts `line` (0-based) at the top of the view and the cursor on `column`.
static void editor_goto_position(Editor* editor, int line, int column) {
  int current_line = buffer_get_current_line_number(editor->current_buffer) - 1;
  int line_diff = line - current_line;
  if (line_diff != 0) {
    buffer_scroll_rows(editor->current_buffer, line_diff);
    editor->start_line = line;
    editor->cursor.y = EDITOR_TOP_BAR_HEIGHT;
    editor_mark_dirty_whole_screen(editor);
  }
  editor_home_cursor_x(editor);
  editor_move_cursor_x(editor, column, false);
}

static bool editor_collect_search_pattern(Editor* editor, int key) {
  if (key == '\n') {
    // Execute search
    if (editor->search_buffer.buffer != NULL &&
        strlen(editor->search_buffer.buffer) > 0) {
      search_set_pattern(&editor->search, editor->search_buffer.buffer,
                         editor->state == EditorState_SearchInputForward);
      search_set_position(&editor->search,
                          buffer_get_current_line_number(editor->current_buffer) -
                            1,
                          editor_get_cursor_x(editor));

      bool found;
      if (editor->state == EditorState_SearchInputForward) {
        found = search_find_next(&editor->search, editor->current_buffer);
      } else {
        found = search_find_prev(&editor->search, editor->current_buffer);
      }

      if (found) {
        // Move cursor to match
        editor_goto_position(editor, editor->search.last_match_line,
                             editor->search.last_match_col);
      } else {
        editor_set_error_message(editor, "Pattern not found");
      }
    }
    command_deinit(&editor->search_buffer);
    editor->state = EditorState_Running;
    return true;
  } else if (key == 27) {
    // Cancel search
    command_deinit(&editor->search_buffer);
    editor->state = EditorState_Running;
    return true;
  } else if (key == KEY_BACKSPACE || key == 127 || key == EDITOR_KEY_CTRL_H) {
    if (editor->search_buffer.cursor_position > 0) {
      editor->search_buffer.cursor_position--;
      editor->search_buffer.buffer[editor->search_buffer.cursor_position] =
        '\0';
    }
    return false;
  }
  if (!editor_is_printable_key(key)) {
    return false;  // terminal noise, not part of the pattern
  }
  command_append(&editor->search_buffer, (char)key);
  return false;
}

// Jump to next search match
static void editor_search_next(Editor* editor) {
  if (editor->search.pattern == NULL) {
    return;
  }

  // Do NOT update position from cursor - continue from last match
  // The search state already tracks where we are
  bool found = search_find_next(&editor->search, editor->current_buffer);

  if (found) {
    editor_goto_position(editor, editor->search.last_match_line,
                         editor->search.last_match_col);
  } else {
    editor_set_error_message(editor, "Pattern not found");
  }
}

// Jump to previous search match
static void editor_search_prev(Editor* editor) {
  if (editor->search.pattern == NULL) {
    return;
  }

  // Do NOT update position from cursor - continue from last match
  // The search state already tracks where we are
  bool found = search_find_prev(&editor->search, editor->current_buffer);

  if (found) {
    editor_goto_position(editor, editor->search.last_match_line,
                         editor->search.last_match_col);
  } else {
    editor_set_error_message(editor, "Pattern not found");
  }
}

static CommandResult editor_process_save_command(Editor* editor) {
  int command_length = strlen(editor->command.buffer);
  const char* filename = buffer_get_filename(editor->current_buffer);
  bool should_exit = false;
  if (command_length > 1) {
    // additional arguments for the command
    if (editor->command.buffer[1] == ' ') {
      // save to a different file
      filename = &editor->command.buffer[2];
      while (isspace(*filename)) {
        filename++;  // Skip leading spaces
      }
      if (strlen(filename) == 0) {
        filename = buffer_get_filename(editor->current_buffer);
      }
    } else if (editor->command.buffer[1] == 'q') {
      // force save
      should_exit = true;
    } else {
      editor_set_error_message(editor, "Invalid command syntax");
      return CommandResult_Error;
    }
  }

  if (filename == NULL || strlen(filename) == 0) {
    editor_set_error_message(editor, "No filename specified for saving");
    return CommandResult_Error;
  }
  FILE* file = fopen(filename, "w");

  if (file == NULL) {
    editor_set_error_message(editor, "Failed to open file for writing");
    return CommandResult_Error;
  }

  bool written = true;
  for (BufferRow* row = buffer_get_first_row(editor->current_buffer); row != NULL;
       row = row->next) {
    if (row->data && fprintf(file, "%s\n", row->data) < 0) {
      written = false;
    }
  }

  if (fclose(file) != 0 || !written) {
    // The autosave stays: it may now be the only whole copy.
    editor_set_error_message(editor, "Failed to write file");
    return CommandResult_Error;
  }

  Buffer* buffer = editor->current_buffer;
  const char* own = buffer_get_filename(buffer);
  if (own == NULL) {
    // ":w name" names an unnamed buffer, as in vim.
    size_t length = strlen(filename) + 1;
    buffer->filename = malloc(length);
    if (buffer->filename != NULL) {
      memcpy(buffer->filename, filename, length);
    }
    own = buffer->filename;
  }
  if (own != NULL && strcmp(own, filename) == 0) {
    buffer_mark_saved(buffer);
    autosave_remove(buffer);
  }
  if (editor->file_manager != NULL && editor->file_manager->visible) {
    file_manager_refresh(editor->file_manager);  // a new file shows up
  }
  editor_set_error_message(editor, buffer->stale_autosave != NULL
                                     ? "Saved; an older autosave is kept: :delautosave"
                                     : "File saved successfully");
  return should_exit ? CommandResult_ShouldExit : CommandResult_Success;
}

// :help -- the built-in help lives in its own buffer, like vim's :help.
// An already-open help buffer is reused, never duplicated.
static bool editor_show_help(Editor* editor) {
  for (size_t i = 0; editor->buffers != NULL && i < editor->number_of_buffers; ++i) {
    const char* filename = buffer_get_filename(editor->buffers[i]);
    if (filename != NULL && strcmp(filename, HELP_BUFFER_NAME) == 0) {
      editor_switch_to_buffer_by_index(editor, i);
      return true;
    }
  }
  Buffer* buffer = help_alloc_buffer();
  if (buffer == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for help");
    return false;
  }
  if (!editor_append_buffer(editor, buffer)) {
    buffer_free(buffer);
    return false;
  }
  editor_switch_to_buffer_by_index(editor, editor->number_of_buffers - 1);
  return true;
}

static CommandResult editor_process_command(Editor* editor) {
  const Command* command = &editor->command;
  if (command->buffer == NULL) {
    // command_init/command_append could not get memory, so nothing the user
    // typed was ever recorded. Say that rather than blaming the command.
    editor_set_error_message(editor, "Out of memory");
    return CommandResult_Error;
  }
  if (strcmp(command->buffer, "q") == 0) {
    // :q in :help, a :man page or the :cl log closes it, like vim's help
    // window; vi exits from a file, or when nothing else is open.
    if (editor->current_buffer != NULL && editor->current_buffer->autosave_disabled &&
        editor->number_of_buffers > 1) {
      editor_close_current_buffer(editor);
      return CommandResult_Success;
    }
    return CommandResult_ShouldExit;
  }

  CommandResult open_result;
  if (editor_process_open_command(editor, &open_result)) {
    return open_result;
  }

  if (command->buffer[0] == 'w') {
    return editor_process_save_command(editor);
  }

  if (strcmp(command->buffer, "bd") == 0 || strcmp(command->buffer, "bdelete") == 0) {
    editor_close_current_buffer(editor);
    return CommandResult_Success;
  }

  // Handle :bN where N is buffer index (1-indexed for user)
  if (command->buffer[0] == 'b' && command->buffer[1] >= '1' && command->buffer[1] <= '9') {
    int target = atoi(&command->buffer[1]) - 1;  // Convert to 0-indexed
    if (target >= 0 && (size_t)target < editor->number_of_buffers) {
      editor_switch_to_buffer_by_index(editor, (size_t)target);
      return CommandResult_Success;
    }
    editor_set_error_message(editor, "Invalid buffer number");
    return CommandResult_Error;
  }

  // Handle :bn (next buffer) and :bp (previous buffer)
  if (strcmp(command->buffer, "bn") == 0) {
    editor_switch_to_next_buffer(editor);
    return CommandResult_Success;
  }
  if (strcmp(command->buffer, "bp") == 0) {
    editor_switch_to_prev_buffer(editor);
    return CommandResult_Success;
  }

  // :man, :make, :set, :cn/:cp/:cc/:cl -- before :s, which takes any 's'
  CommandResult build_result;
  if (editor_process_build_command(editor, &build_result)) {
    return build_result;
  }

  // Handle substitution commands: :s/old/new/flags
  if (command->buffer[0] == 's') {
    // Parse substitution pattern: s/pattern/replacement/flags
    const char* cmd = command->buffer + 1;  // Skip 's'
    if (*cmd != '/') {
      editor_set_error_message(editor, "Invalid substitute syntax");
      return CommandResult_Error;
    }
    cmd++;  // Skip first '/'

    // Find pattern end
    const char* pattern_start = cmd;
    const char* pattern_end = strchr(cmd, '/');
    if (pattern_end == NULL) {
      editor_set_error_message(editor, "Invalid substitute syntax");
      return CommandResult_Error;
    }

    size_t pattern_len = pattern_end - pattern_start;
    char* pattern = (char*)malloc(pattern_len + 1);
    if (pattern == NULL) {
      editor_set_error_message(editor, "Memory allocation failed");
      return CommandResult_Error;
    }
    strncpy(pattern, pattern_start, pattern_len);
    pattern[pattern_len] = '\0';

    // Find replacement end
    const char* replacement_start = pattern_end + 1;
    const char* replacement_end = strchr(replacement_start, '/');
    if (replacement_end == NULL) {
      replacement_end = replacement_start + strlen(replacement_start);
    }

    size_t replacement_len = replacement_end - replacement_start;
    char* replacement = (char*)malloc(replacement_len + 1);
    if (replacement == NULL) {
      free(pattern);
      editor_set_error_message(editor, "Memory allocation failed");
      return CommandResult_Error;
    }
    strncpy(replacement, replacement_start, replacement_len);
    replacement[replacement_len] = '\0';

    // Check for flags
    bool global = false;
    if (replacement_end != NULL && *(replacement_end + 1) != '\0') {
      const char* flags = replacement_end + 1;
      if (strchr(flags, 'g') != NULL) {
        global = true;
      }
    }

    // Execute substitution on current line
    int current_line =
      buffer_get_current_line_number(editor->current_buffer) - 1;
    ReplaceResult result = search_replace_line(
      editor->current_buffer, current_line, pattern, replacement, global);

    if (result.replacements > 0) {
      static char msg[64];
      snprintf(msg, sizeof(msg), "Replaced %d occurrence(s)",
               result.replacements);
      editor_note_change(editor);
      editor_set_error_message(editor, msg);
      editor_mark_dirty_from_cursor(editor);
    } else {
      editor_set_error_message(editor, "Pattern not found");
    }

    free(pattern);
    free(replacement);
    return CommandResult_Success;
  }

  // Handle %s substitution: :%s/old/new/g
  if (command->buffer[0] == '%' && command->buffer[1] == 's') {
    const char* cmd = command->buffer + 2;  // Skip '%s'
    if (*cmd != '/') {
      editor_set_error_message(editor, "Invalid substitute syntax");
      return CommandResult_Error;
    }
    cmd++;  // Skip first '/'

    // Find pattern end
    const char* pattern_start = cmd;
    const char* pattern_end = strchr(cmd, '/');
    if (pattern_end == NULL) {
      editor_set_error_message(editor, "Invalid substitute syntax");
      return CommandResult_Error;
    }

    size_t pattern_len = pattern_end - pattern_start;
    char* pattern = (char*)malloc(pattern_len + 1);
    if (pattern == NULL) {
      editor_set_error_message(editor, "Memory allocation failed");
      return CommandResult_Error;
    }
    strncpy(pattern, pattern_start, pattern_len);
    pattern[pattern_len] = '\0';

    // Find replacement end
    const char* replacement_start = pattern_end + 1;
    const char* replacement_end = strchr(replacement_start, '/');
    if (replacement_end == NULL) {
      replacement_end = replacement_start + strlen(replacement_start);
    }

    size_t replacement_len = replacement_end - replacement_start;
    char* replacement = (char*)malloc(replacement_len + 1);
    if (replacement == NULL) {
      free(pattern);
      editor_set_error_message(editor, "Memory allocation failed");
      return CommandResult_Error;
    }
    strncpy(replacement, replacement_start, replacement_len);
    replacement[replacement_len] = '\0';

    // Check for flags
    bool global = false;
    if (replacement_end != NULL && *(replacement_end + 1) != '\0') {
      const char* flags = replacement_end + 1;
      if (strchr(flags, 'g') != NULL) {
        global = true;
      }
    }

    // Execute substitution on entire buffer
    ReplaceResult result = search_replace_all(
      editor->current_buffer, pattern, replacement, global);

    if (result.replacements > 0) {
      static char msg[64];
      snprintf(msg, sizeof(msg), "Replaced %d occurrence(s) in %d line(s)",
               result.replacements, result.lines_affected);
      editor_note_change(editor);
      editor_set_error_message(editor, msg);
      editor_mark_dirty_whole_screen(editor);
    } else {
      editor_set_error_message(editor, "Pattern not found");
    }

    free(pattern);
    free(replacement);
    return CommandResult_Success;
  }

  // :recover -- take the text an autosave left by a crash kept
  if (strcmp(command->buffer, "rec") == 0 || strcmp(command->buffer, "recover") == 0) {
    if (editor->current_buffer == NULL ||
        editor->current_buffer->stale_autosave == NULL) {
      editor_set_error_message(editor, "No autosave to recover");
      return CommandResult_Error;
    }
    if (!autosave_recover(editor->current_buffer)) {
      editor_set_error_message(editor, "Cannot read the autosave");
      return CommandResult_Error;
    }
    editor->start_line = 0;
    editor->start_column = 0;
    editor_home_cursor_xy(editor);
    editor_mark_dirty_whole_screen(editor);
    editor_set_error_message(editor, "Recovered; :w keeps it");
    return CommandResult_Success;
  }
  // :delautosave -- drop that autosave instead
  if (strcmp(command->buffer, "delautosave") == 0) {
    if (!autosave_discard(editor->current_buffer)) {
      editor_set_error_message(editor, "No autosave to delete");
      return CommandResult_Error;
    }
    editor_set_error_message(editor, "Autosave deleted");
    return CommandResult_Success;
  }

  // :tag NAME / :ta NAME -- jump to a ctags definition
  if (strncmp(command->buffer, "tag", 3) == 0 || strncmp(command->buffer, "ta", 2) == 0) {
    const char* arg = command->buffer + (command->buffer[2] == 'g' ? 3 : 2);
    if (*arg != ' ' && *arg != '\0') {
      editor_set_error_message(editor, "Command not found");
      return CommandResult_Error;
    }
    while (*arg == ' ') {
      ++arg;
    }
    if (*arg == '\0') {
      editor_set_error_message(editor, "Argument required");
      return CommandResult_Error;
    }
    return editor_jump_to_tag(editor, arg) ? CommandResult_Success
                                           : CommandResult_Error;
  }
  if (strcmp(command->buffer, "pop") == 0 || strcmp(command->buffer, "po") == 0) {
    return editor_pop_tag(editor) ? CommandResult_Success : CommandResult_Error;
  }

  // :help and its unambiguous prefixes h/he/hel -- the built-in help
  if (strcmp(command->buffer, "h") == 0 || strcmp(command->buffer, "he") == 0 ||
      strcmp(command->buffer, "hel") == 0 || strcmp(command->buffer, "help") == 0) {
    return editor_show_help(editor) ? CommandResult_Success : CommandResult_Error;
  }

  if (command->buffer[0] == '\0') {
    return CommandResult_Success;  // bare ':' -- vi does nothing
  }

  editor_set_error_message(editor, "Command not found");
  return CommandResult_Error;
}

static void editor_set_error_message(Editor* editor, const char* message) {
  int error_length = strlen(message);
  int message_offset = 0;
  editor_clear_error_message(editor);
  editor->error_message =
    (char*)malloc(editor->command.cursor_position + error_length + 5);
  if (editor->error_message && editor->command.buffer) {
    memcpy(editor->error_message, message, error_length);
    message_offset += error_length;
    memcpy(editor->error_message + message_offset, ": '", 3);
    message_offset += 3;
    memcpy(editor->error_message + message_offset, editor->command.buffer,
           editor->command.cursor_position);
    message_offset += editor->command.cursor_position;
    editor->error_message[message_offset] = '\'';
    ++message_offset;
    editor->error_message[message_offset] = '\0';
  } else if (editor->error_message != NULL) {
    memcpy(editor->error_message, message, error_length);
    editor->error_message[error_length] = '\0';
  }
  editor_redraw_screen(editor);
}

static void editor_restore_cursor_position(const Editor* editor) {
  move(editor->cursor.y, editor->cursor.x);
}

static void editor_move_cursor_to_start(Editor* editor) {
  const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  editor_home_cursor_x(editor);
  editor_move_cursor_x(editor, buffer_row_get_offset_to_first_char(current_row, 0),
                       false);
}

static void editor_move_to_top(Editor* editor) {
  if (buffer_current_is_first_row(editor->current_buffer)) {
    return;  // Already at the first row
  }
  buffer_scroll_to_top(editor->current_buffer);
  editor_home_cursor_xy(editor);
}

static void editor_move_to_bottom(Editor* editor) {
  const int number_of_lines = buffer_get_number_of_lines(editor->current_buffer);
  const int lines_to_the_end = number_of_lines - editor->start_line - 1;
  if (buffer_current_is_last_row(editor->current_buffer)) {
    return;  // Already at the last row
  }
  editor_move_cursor_y(editor, lines_to_the_end);
  buffer_scroll_rows(editor->current_buffer, lines_to_the_end);
  editor_fix_cursor_position(editor);
}

static void editor_fix_cursor_position(Editor* editor) {
  const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  int line_length = buffer_row_get_length(current_row);

  if (line_length <= editor_get_cursor_x(editor)) {
    editor_home_cursor_x(editor);
    editor_move_cursor_x(editor, line_length, false);
  }
}

static void editor_move_cursor_x_to_right(Editor* editor, int x, bool insert_mode) {
  BufferRow* current_line = buffer_get_current_line(editor->current_buffer);
  const int line_length = buffer_row_get_length(current_line) - 1 + insert_mode;
  const int chars_till_end = line_length - editor_get_cursor_x(editor);
  const int chars_till_window_end = editor->window.width - editor->cursor.x - 1;
  if (x > chars_till_end) {
    x = chars_till_end;
  }

  if (x > chars_till_window_end) {
    editor->cursor.x = editor->window.width - 1;
    x -= chars_till_window_end;
  } else {
    editor->cursor.x += x;
    x = 0;
  }

  if (x > 0) {
    editor->start_column += x;
  }
}

static void editor_move_cursor_x_to_left(Editor* editor, int x) {
  const int chars_till_window = editor->cursor.x - editor->number_of_line_digits;

  if (x > chars_till_window) {
    x -= chars_till_window;
    editor->cursor.x = editor->number_of_line_digits;
  } else {
    editor->cursor.x -= x;
    x = 0;
  }

  if (x > editor->start_column) {
    editor->start_column = 0;
  } else {
    editor->start_column -= x;
  }
}

static void editor_move_cursor_x(Editor* editor, int x, bool insert_mode) {
  if (x >= 0) {
    editor_move_cursor_x_to_right(editor, x, insert_mode);
  } else {
    editor_move_cursor_x_to_left(editor, -x);
  }
}

static void editor_move_cursor_y(Editor* editor, int y) {
  const int previous_start = editor->start_line;
  editor->cursor.y += y;
  if (editor->cursor.y <= EDITOR_TOP_BAR_HEIGHT) {
    // Calculate how many lines past the top we went
    int lines_past_top = EDITOR_TOP_BAR_HEIGHT - editor->cursor.y + 1;
    editor->start_line -= lines_past_top;
    if (editor->start_line < 0) {
      editor->start_line = 0;
    }
    editor->cursor.y = EDITOR_TOP_BAR_HEIGHT;
  } else if (editor->cursor.y >= editor->window.height - editor_bottom_height(editor)) {
    // Cursor went past the last content row
    const int number_of_lines = buffer_get_number_of_lines(editor->current_buffer);
    editor->start_line +=
      (editor->cursor.y - (editor->window.height - editor_bottom_height(editor) - 1));
    editor->cursor.y = editor->window.height - editor_bottom_height(editor) - 1;
    if (editor->start_line > number_of_lines - editor->window.height +
                               editor_bottom_height(editor) + EDITOR_TOP_BAR_HEIGHT) {
      editor->start_line = number_of_lines - editor->window.height +
                           editor_bottom_height(editor) + EDITOR_TOP_BAR_HEIGHT;
    }
  }
  if (editor->start_line != previous_start) {
    editor_mark_dirty_whole_screen(editor);
  }
}

static void editor_move_cursor_to_end(Editor* editor) {
  const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  editor_move_cursor_x(editor, buffer_row_get_length(current_row), false);
}

static void editor_process_editor_key(Editor* editor, int key) {
  // TODO: scroll buffers
  Buffer* current_buffer = editor->current_buffer;
  switch (key) {
    case 'h':
    case KEY_LEFT: {
      // Move cursor left
      editor->end_line_mode = false;
      editor_move_cursor_x(editor, -1, false);
      return;
    }
    case 'l':
    case KEY_RIGHT: {
      // Move cursor right
      editor_move_cursor_x(editor, 1, false);
      return;
    }
    case 'j':
    case KEY_DOWN: {
      // Move cursor down
      if (buffer_current_is_last_row(current_buffer)) {
        return;
      }
      editor_move_cursor_y(editor, 1);
      buffer_scroll_rows(editor->current_buffer, 1);
      editor_fix_cursor_position(editor);
      return;
    }
    case 'k':
    case KEY_UP: {
      // Move cursor up
      if (buffer_current_is_first_row(current_buffer)) {
        return;
      }
      editor_move_cursor_y(editor, -1);
      buffer_scroll_rows(editor->current_buffer, -1);
      editor_fix_cursor_position(editor);
      return;
    }
    case '^': {
      editor->end_line_mode = false;
      editor_move_cursor_to_start(editor);
      return;
    }
    case '$': {
      editor_move_cursor_to_end(editor);
      editor->end_line_mode = true;
      return;
    }
    case 'G': {
      editor->end_line_mode = false;
      editor_move_to_bottom(editor);
      return;
    }
    case 'w': {
      const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
      int offset_to_word =
        buffer_row_get_offset_to_next_word(current_row, editor_get_cursor_x(editor));

      editor_move_cursor_x(editor, offset_to_word, false);
      return;
    }
    case 'e': {
      const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
      int offset_to_end =
        buffer_row_get_offset_to_end_of_word(current_row, editor_get_cursor_x(editor));

      editor_move_cursor_x(editor, offset_to_end, false);
      return;
    }
    case 'b': {
      const BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
      int offset_to_word =
        buffer_row_get_offset_to_prev_word(current_row, editor_get_cursor_x(editor));
      editor_move_cursor_x(editor, offset_to_word, false);
      return;
    }
    case '/': {
      // Start forward search
      editor_clear_error_message(editor);
      if (editor->search_buffer.buffer == NULL) {
        command_init(&editor->search_buffer);
      } else {
        editor->search_buffer.cursor_position = 0;
        editor->search_buffer.buffer[0] = '\0';
      }
      editor->state = EditorState_SearchInputForward;
      return;
    }
    case '?': {
      // Start backward search
      editor_clear_error_message(editor);
      if (editor->search_buffer.buffer == NULL) {
        command_init(&editor->search_buffer);
      } else {
        editor->search_buffer.cursor_position = 0;
        editor->search_buffer.buffer[0] = '\0';
      }
      editor->state = EditorState_SearchInputBackward;
      return;
    }
    case 'n': {
      // Find next match
      editor_search_next(editor);
      return;
    }
    case 'N': {
      // Find previous match
      editor_search_prev(editor);
      return;
    }
    case 0x1d: {  // Ctrl-]
      char* word = editor_word_under_cursor(editor);
      if (word == NULL) {
        editor_set_error_message(editor, "No identifier under cursor");
        return;
      }
      editor_jump_to_tag(editor, word);
      free(word);
      return;
    }
    case 0x14: {  // Ctrl-T
      editor_pop_tag(editor);
      return;
    }
    case 'K': {  // the man page of the word under the cursor, as in vim
      editor_man_under_cursor(editor);
      return;
    }
    case 'g': {
      editor->key_sequence[0] = 'g';
      return;
    }
    case 'd': {
      editor->key_sequence[0] = 'd';
      return;
    }
    case 'x': {
      BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
      if (buffer_row_remove_char(current_row, editor_get_cursor_x(editor))) {
        editor_note_change(editor);
        editor_fix_cursor_position(editor);
      }
      return;
    }
    case 'i': {
      editor->end_line_mode = false;
      editor->state = EditorState_EditMode;
      return;
    }
    case 'a': {
      editor->end_line_mode = false;
      editor->state = EditorState_EditMode;
      editor_move_cursor_x(editor, 1, true);
      return;
    }
    case 'o': {  // a new line below, indented (autoindent)
      editor->end_line_mode = false;
      editor->state = EditorState_EditMode;
      const BufferRow* row = buffer_get_current_line(editor->current_buffer);
      editor_set_cursor_column(editor, row != NULL ? row->len : 0);
      editor_insert_newline(editor);
      return;
    }
    case 'O': {  // a new line above
      editor->end_line_mode = false;
      editor->state = EditorState_EditMode;
      editor_open_line_above(editor);
      return;
    }
    case 27: {
      editor_clear_error_message(editor);
      return;
    }
    default: {
      if (key >= '0' && key <= '9') {
        editor_process_key_sequence(editor, key);
      }
      return;
    }
  }
}

static bool editor_append_buffer(Editor* editor, Buffer* buffer) {
  Buffer** new_buffers = (Buffer**)realloc(
    editor->buffers, sizeof(Buffer*) * (editor->number_of_buffers + 1));
  if (new_buffers == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffers");
    return false;
  }
  editor->buffers = new_buffers;
  if (editor->buffers == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffers");
    return false;
  }

  editor->buffers[editor->number_of_buffers] = buffer;
  editor->number_of_buffers++;
  return true;
}

static int count_digits(int number) {
  int count = 0;
  if (number == 0) {
    return 1;
  }
  while (number > 0) {
    number /= 10;
    count++;
  }
  return count;
}

static attr_t editor_get_highlight_style(EHighlightToken token) {
  if (token < 0 || token >= EHighlightToken_Count) {
    token = EHighlightToken_Normal;
  }

  switch (token) {
    case EHighlightToken_Keyword:
      return COLOR_KEYWORD | A_BOLD;
    case EHighlightToken_String:
      return COLOR_STRING;
    case EHighlightToken_Comment:
      return COLOR_COMMENT;
    case EHighlightToken_Type:
      return COLOR_TYPE | A_BOLD;
    case EHighlightToken_Preprocessor:
      return COLOR_PREPROCESSOR;
    case EHighlightToken_Digit:
      return COLOR_NUMBER | A_BOLD;
    case EHighlightToken_Symbol:
      return COLOR_STRING | A_BOLD;
    case EHighlightToken_Keyword2:
      return COLOR_TYPE | A_BOLD;
    case EHighlightToken_Symbol2:
      return COLOR_NUMBER | A_BOLD;
    case EHighlightToken_Normal:
    case EHighlightToken_Count:
    default:
      return COLOR_OTHER;
  }
}

static void editor_decorate_and_draw_line(Editor* editor,
                                          int line_number,
                                          BufferRow* row,
                                          int line_number_width) {
  const char* line = &row->data[editor->start_column];
  const unsigned char* hl =
    (const unsigned char*)&row->highlight_data[editor->start_column];
  const int max_columns = editor->window.width - line_number_width;

  if (max_columns <= 0) {
    return;
  }

  move(line_number, line_number_width);
  attrset(COLOR_OTHER);

  EHighlightToken token = EHighlightToken_Normal;
  for (int i = 0; i < row->len - editor->start_column && i < max_columns; ++i) {
    if (line[i] == '\0') {
      break;
    }
    if (token != (EHighlightToken)hl[i]) {
      token = (EHighlightToken)hl[i];
      attrset(editor_get_highlight_style(token));
    }
    addch(line[i]);
  }

  attrset(COLOR_OTHER);
}

int editor_get_content_offset_x(const Editor* editor) {
  if (editor->file_manager && editor->file_manager->visible) {
    return editor->file_manager->width;
  }
  return 0;
}

static void editor_draw_buffers(Editor* editor) {
  int line_number = EDITOR_TOP_BAR_HEIGHT;
  editor->string_rendering_ongoing = false;
  editor->multiline_comment_ongoing = false;

  // Calculate content offset based on file manager visibility
  int content_offset = editor_get_content_offset_x(editor);
  editor->editor_offset_x = content_offset;

  if (editor->end_line_mode) {
    editor_move_cursor_to_end(editor);
  }
  if (editor->current_buffer != NULL) {
    // Draw current buffer
    // Content area: from EDITOR_TOP_BAR_HEIGHT to (window.height - editor_bottom_height(editor))
    const int window_height = editor->window.height - editor_bottom_height(editor);
    BufferRow* row = buffer_get_row(editor->current_buffer, editor->start_line);
    // Fixed 4-digit line number width to prevent content shifting
    // when line count goes from 99 to 100, 999 to 1000, etc.
    (void)count_digits;  // Unused now, but kept for potential future use
    editor->number_of_line_digits = 4 + content_offset;
    if (editor->cursor.x <= editor->number_of_line_digits) {
      editor->cursor.x = editor->number_of_line_digits;
    }

    while (line_number < window_height) {
      // Calculate actual row number: subtract top bar height and add start_line offset
      int row_number = line_number - EDITOR_TOP_BAR_HEIGHT + 1 + editor->start_line;

      if (row != NULL && row->dirty) {
        static char line_buffer[32];
        itoa(row_number, line_buffer, 10);
        int line_length = strlen(line_buffer);
        for (int i = line_length; i < editor->number_of_line_digits - content_offset; ++i) {
          line_buffer[i] = ' ';
        }
        line_buffer[editor->number_of_line_digits - content_offset] = '\0';

        attrset(COLOR_OTHER);
        mvaddstr(line_number, content_offset, line_buffer);
        if (editor->start_column < buffer_row_get_length(row)) {
          editor_decorate_and_draw_line(editor, line_number, row,
                                        editor->number_of_line_digits);
        }
        attrset(COLOR_OTHER);
        clrtoeol();
        row->dirty = false;
        row = row->next;
      } else if (row != NULL) {
        row = row->next;
      } else if (row == NULL) {
        move(line_number, content_offset);
        clrtoeol();
      }

      line_number++;
    }
  }
}

static void editor_draw_file_manager(Editor* editor) {
  if (!editor->file_manager || !editor->file_manager->visible) {
    return;
  }

  const int window_height =
    editor->window.height - editor_bottom_height(editor) - EDITOR_TOP_BAR_HEIGHT + 1;

  editor->file_manager->focused = editor->state == EditorState_FileManager;
  file_manager_draw(editor->file_manager, 1, window_height);
}

static void editor_process_gkey_sequence(Editor* editor, int key) {
  if (key == 'g') {
    editor->key_sequence[0] = 0;
    editor_move_to_top(editor);
    editor_fix_cursor_position(editor);
  } else if (key == 't') {
    editor->key_sequence[0] = 0;
    // If repeat_count is set, switch to that buffer index directly
    // e.g., 2gt switches to buffer 2 (1-indexed for user)
    if (editor->repeat_count > 0) {
      size_t target_index = (size_t)editor->repeat_count;
      if (target_index < editor->number_of_buffers) {
        editor_switch_to_buffer_by_index(editor, target_index);
      } else {
        editor_set_error_message(editor, "Buffer index out of range");
      }
      editor->repeat_count = 0;
    } else {
      editor_switch_to_next_buffer(editor);
    }
  } else if (key == 'T') {
    editor->key_sequence[0] = 0;
    editor_switch_to_prev_buffer(editor);
  } else {
    editor->key_sequence[0] = 0;
  }
}

static void editor_process_dkey_sequence(Editor* editor, int key) {
  BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  const int number_of_lines = buffer_get_number_of_lines(editor->current_buffer);
  switch (key) {
    case 'd': {
      if (number_of_lines <= 1) {
        buffer_row_replace_line(current_row, "\n");
      } else {
        int offset = buffer_remove_current_row(editor->current_buffer);

        if (offset < 0) {
          editor_move_cursor_y(editor, -1);
        }
      }
      editor_note_change(editor);
      editor_mark_dirty_from_cursor(editor);
    } break;
    case 'w': {
      // Delete word
      int offset_to_word =
        buffer_row_get_offset_to_next_word(current_row, editor_get_cursor_x(editor));
      if (offset_to_word > 0) {
        buffer_row_remove_chars(current_row, editor_get_cursor_x(editor),
                                offset_to_word);
        editor_note_change(editor);
      }
    } break;
    case 'e': {
      // Delete to end of word
      int offset_to_end =
        buffer_row_get_offset_to_end_of_word(current_row, editor_get_cursor_x(editor));
      if (offset_to_end > 0) {
        buffer_row_remove_chars(current_row, editor_get_cursor_x(editor),
                                offset_to_end + 1);  // +1 to include current char
        editor_note_change(editor);
      }
    } break;
  }

  editor_fix_cursor_position(editor);
  editor->key_sequence[0] = 0;
}

static bool editor_process_key_sequence(Editor* editor, int key) {
  const size_t current_length = strlen(editor->key_sequence);
  if (current_length >= sizeof(editor->key_sequence) - 1) {
    editor->key_sequence[0] = '\0';
    return true;
  }

  if (editor->key_sequence[0] == 0 ||
      (editor->key_sequence[0] >= '0' && editor->key_sequence[0] <= '9')) {
    if (key >= '0' && key <= '9') {
      editor->key_sequence[current_length] = (char)key;
      editor->key_sequence[current_length + 1] = '\0';
      return true;
    } else {
      editor->repeat_count = atoi(editor->key_sequence) - 1;
      editor->key_sequence[0] = '\0';
      return false;
    }
  }
  switch (key) {
    case 27: {
      editor->key_sequence[0] = '\0';
      return true;
    }
    default: {
      if (editor->key_sequence[0] == 'g') {
        editor_process_gkey_sequence(editor, key);
        return true;
      } else if (editor->key_sequence[0] == 'd') {
        editor_process_dkey_sequence(editor, key);
        return true;
      }
      editor->key_sequence[0] = 0;
      return true;
    }
  }
  return true;
}

// Replaces the text the completion last put in the row with `text`, leaving
// the cursor after it.
static void editor_completion_replace(Editor* editor, const char* text) {
  Completion* completion = editor->completion;
  const int length = (int)strlen(text);
  if (completion->inserted_len > 0) {
    buffer_row_remove_chars(completion->row, completion->start, completion->inserted_len);
  }
  if (length > 0) {
    buffer_row_insert_chars(completion->row, completion->start, text, length);
  }
  editor_move_cursor_x(editor, length - completion->inserted_len, true);
  completion->inserted_len = length;
  editor_note_change(editor);
}

// Ctrl-N (step +1) / Ctrl-P (step -1) in insert mode: the first press
// gathers the candidates for the word before the cursor, every press puts
// the next (previous) one in its place, and the typed word comes back round
// after the last.
// The C library's tags file (YASVI_SYSTEM_TAGS, made by build_rootfs.sh from
// the headers in /usr/include) when the current buffer is C and it exists,
// else NULL: libc's functions, macros and types for completion and Ctrl-].
static const char* editor_system_tags(const Editor* editor) {
  const Buffer* buffer = editor->current_buffer;
  if (buffer == NULL || buffer->filetype == NULL ||
      strcmp(buffer->filetype->name, "c") != 0 || access(YASVI_SYSTEM_TAGS, R_OK) != 0) {
    return NULL;
  }
  return YASVI_SYSTEM_TAGS;
}

static void editor_complete(Editor* editor, int step) {
  if (editor->current_buffer == NULL) {
    return;
  }
  if (editor->completion == NULL) {
    editor->completion = (Completion*)calloc(1, sizeof(Completion));
    if (editor->completion == NULL) {
      return;
    }
    editor->completion->selected = -1;
  }
  Completion* completion = editor->completion;
  if (!completion->active) {
    BufferRow* row = buffer_get_current_line(editor->current_buffer);
    if (row == NULL) {
      return;
    }
    const Filetype* filetype = editor->current_buffer->filetype;
    const int cursor = editor_get_cursor_x(editor);
    const int start = completion_word_start(row->data, row->len, cursor,
                                            filetype != NULL ? filetype->word_prefix_chars : NULL);
    char* prefix = (char*)malloc((size_t)(cursor - start) + 1);
    if (prefix == NULL) {
      return;
    }
    memcpy(prefix, &row->data[start], (size_t)(cursor - start));
    prefix[cursor - start] = '\0';

    char* tags_path = tags_find_file(buffer_get_filename(editor->current_buffer));
    CompletionSources sources = {
      .current = editor->current_buffer,
      .others = (const Buffer* const*)editor->buffers,
      .number_of_others = editor->number_of_buffers,
      .tags_path = tags_path,
      .system_tags_path = editor_system_tags(editor),
    };
    completion_collect(&completion->list, prefix, start, &sources);
    free(tags_path);
    if (completion->list.count == 0) {
      free(prefix);
      editor_set_error_message(editor, "No completions");
      return;
    }
    if (editor->error_message) {
      editor_clear_error_message(editor);
    }
    completion->active = true;
    completion->row = row;
    completion->start = start;
    completion->original = prefix;
    completion->inserted_len = cursor - start;
    completion->selected = -1;
  }
  editor_completion_replace(editor, completion_step(completion, step));
  // The menu is drawn over the rows around the cursor; redraw them all so a
  // shorter or moved menu leaves nothing behind.
  editor_mark_dirty_whole_screen(editor);
}

// Ends a completion, keeping the chosen text or, with `restore`, putting the
// typed word back.
static void editor_completion_end(Editor* editor, bool restore) {
  Completion* completion = editor->completion;
  if (completion == NULL || !completion->active) {
    return;
  }
  if (restore) {
    editor_completion_replace(editor, completion->original);
  }
  completion_reset(completion);
  editor_mark_dirty_whole_screen(editor);
}

// The candidate menu: under the word when it fits there, else above it. The
// selected entry is drawn in normal video in a reverse-video list, like vi's.
static void editor_draw_completion_menu(Editor* editor) {
  const Completion* completion = editor->completion;
  const int count = completion->list.count;
  const int top = EDITOR_TOP_BAR_HEIGHT;
  const int bottom = editor->window.height - editor_bottom_height(editor);
  const int space_below = bottom - (editor->cursor.y + 1);
  const int space_above = editor->cursor.y - top;

  int rows = count < EDITOR_COMPLETION_MENU_ROWS ? count : EDITOR_COMPLETION_MENU_ROWS;
  int y;
  if (rows <= space_below) {
    y = editor->cursor.y + 1;
  } else if (space_above > space_below) {
    rows = rows < space_above ? rows : space_above;
    y = editor->cursor.y - rows;
  } else {
    rows = space_below;
    y = editor->cursor.y + 1;
  }
  if (rows <= 0) {
    return;
  }

  int width = 0;
  for (int i = 0; i < count; ++i) {
    const int length = (int)strlen(completion->list.items[i]);
    width = length > width ? length : width;
  }
  width += 2;  // a space either side
  int x = editor->cursor.x - completion->inserted_len;
  if (x < editor->number_of_line_digits) {
    x = editor->number_of_line_digits;
  }
  if (width > editor->window.width) {
    width = editor->window.width;
  }
  if (x + width > editor->window.width) {
    x = editor->window.width - width;
  }

  const int first = completion->selected >= rows ? completion->selected - rows + 1 : 0;
  char line[256];
  if (width > (int)sizeof(line) - 1) {
    width = (int)sizeof(line) - 1;
  }
  for (int i = 0; i < rows && first + i < count; ++i) {
    const int index = first + i;
    const char* item = completion->list.items[index];
    int length = (int)strlen(item);
    if (length > width - 2) {
      length = width - 2;
    }
    line[0] = ' ';
    memcpy(&line[1], item, (size_t)length);
    memset(&line[1 + length], ' ', (size_t)(width - 1 - length));
    line[width] = '\0';
    attrset(index == completion->selected ? (A_BOLD | COLOR_OTHER) : A_REVERSE);
    mvaddnstr(y + i, x, line, width);
  }
  attrset(COLOR_OTHER);
}

// ============================================================================
// Automatic indentation (config.autoindent, indent.h)
// ============================================================================

// The row Enter, o or O last indented, while nothing else was typed: Esc or
// another Enter on it leaves it empty rather than holding only spaces.
static BufferRow* g_autoindent_row;

#define EDITOR_INDENT_MAX 256

static IndentLanguage editor_indent_language(const Editor* editor) {
  const Buffer* buffer = editor->current_buffer;
  if (buffer == NULL || buffer->filetype == NULL) {
    return IndentLanguage_None;
  }
  return indent_language(buffer->filetype->name);
}

// The cursor to `column` of the current line.
static void editor_set_cursor_column(Editor* editor, int column) {
  editor_home_cursor_x(editor);
  if (column > 0) {
    editor_move_cursor_x(editor, column, true);
  }
}

// One indent step of the current buffer into `out` (EDITOR_INDENT_MAX).
static int editor_indent_unit(const Editor* editor, char* out) {
  return indent_unit(buffer_get_first_row(editor->current_buffer), editor->tab_size, out,
                     EDITOR_INDENT_MAX);
}

// Appends `count` characters of `text` to `out` holding *length of them.
static void editor_indent_append(char* out, int* length, const char* text, int count) {
  while (count-- > 0 && *length < EDITOR_INDENT_MAX - 1) {
    out[(*length)++] = *text++;
  }
  out[*length] = '\0';
}

// The spaces Tab inserts at column `x`: tab_size, or in the indentation with
// autoindent on, up to the next indent step.
static int editor_indent_tab_count(const Editor* editor, const BufferRow* row, int x) {
  const int fallback = editor->tab_size > 0 ? editor->tab_size : 1;
  if (!editor->config.autoindent || row == NULL || x > row->len ||
      indent_leading(row->data, x) != x || memchr(row->data, '\t', (size_t)x) != NULL) {
    return fallback;
  }
  char unit[EDITOR_INDENT_MAX];
  const int step = editor_indent_unit(editor, unit);
  if (step == 0 || unit[0] == '\t') {
    return fallback;
  }
  return step - x % step;
}

// The characters Backspace removes before column `x` (> 0): one, or in an
// indentation of spaces with autoindent on, back to the previous step.
static int editor_indent_backspace_count(const Editor* editor,
                                         const BufferRow* row,
                                         int x) {
  if (!editor->config.autoindent || row == NULL || x > row->len ||
      indent_leading(row->data, x) != x || memchr(row->data, '\t', (size_t)x) != NULL) {
    return 1;
  }
  char unit[EDITOR_INDENT_MAX];
  const int step = editor_indent_unit(editor, unit);
  if (step == 0 || unit[0] == '\t') {
    return 1;
  }
  return (x - 1) % step + 1;
}

// After `row`'s character at `x` was typed: a closing bracket that is the
// first thing on its line moves under the line with its opening bracket.
// Returns the column for the cursor.
static int editor_indent_closer(Editor* editor, BufferRow* row, int x) {
  const IndentLanguage language = editor_indent_language(editor);
  if (!editor->config.autoindent || language == IndentLanguage_None || row == NULL ||
      x >= row->len || !indent_starts_with_closer(row->data + x, 1) ||
      indent_leading(row->data, x) != x) {
    return x + 1;
  }
  const BufferRow* opener = indent_find_opener(language, row, x);
  if (opener == NULL) {
    return x + 1;
  }
  const int target = indent_leading(opener->data, opener->len);
  if (target == x && strncmp(opener->data, row->data, (size_t)x) == 0) {
    return x + 1;
  }
  char indent[EDITOR_INDENT_MAX];
  int length = 0;
  editor_indent_append(indent, &length, opener->data, target);
  buffer_row_remove_chars(row, 0, x);
  buffer_row_insert_chars(row, 0, indent, length);
  return length + 1;
}

// Moves the cursor down to the line below, at `column`.
static void editor_cursor_to_next_line(Editor* editor, int column) {
  editor_move_cursor_y(editor, 1);
  buffer_scroll_rows(editor->current_buffer, 1);
  editor_set_cursor_column(editor, column);
}

// Enter in insert mode (and o). With autoindent the new line gets the
// indentation of the old one, a step more after a line that opens a block;
// text moved down loses its leading white space, a closing bracket at its
// start goes under its opener, and Enter between { and } leaves the cursor
// on an indented line of its own with } below it. The old line loses its
// trailing white space.
static void editor_insert_newline(Editor* editor) {
  Buffer* buffer = editor->current_buffer;
  BufferRow* row = buffer_get_current_line(buffer);
  const int x = editor_get_cursor_x(editor);
  editor_mark_dirty_from_cursor(editor);
  if (!editor->config.autoindent || row == NULL) {
    buffer_break_current_line(buffer, x);
    editor_note_change(editor);
    editor_cursor_to_next_line(editor, 0);
    return;
  }
  const IndentLanguage language = editor_indent_language(editor);
  const int split = x < row->len ? x : row->len;
  char indent[EDITOR_INDENT_MAX];
  int indent_length = 0;
  const int base = indent_leading(row->data, split);
  editor_indent_append(indent, &indent_length, row->data, base);
  const bool opens = indent_opens_block(language, row->data, split);
  if (opens) {
    char unit[EDITOR_INDENT_MAX];
    const int unit_length = editor_indent_unit(editor, unit);
    editor_indent_append(indent, &indent_length, unit, unit_length);
  }
  int keep = split;
  while (keep > 0 && (row->data[keep - 1] == ' ' || row->data[keep - 1] == '\t')) {
    --keep;
  }

  buffer_break_current_line(buffer, split);
  buffer_row_trim(row, keep);
  BufferRow* moved = row->next;
  if (moved == NULL) {
    editor_note_change(editor);
    return;
  }
  buffer_row_remove_chars(moved, 0, indent_leading(moved->data, moved->len));
  const bool closer =
    language != IndentLanguage_None && indent_starts_with_closer(moved->data, moved->len);
  if (opens && closer) {
    // { | }: the } line keeps the old indentation, the cursor gets a line
    buffer_row_insert_chars(moved, 0, row->data, base);
    buffer_break_current_line(buffer, row->len);
    moved = row->next;
    buffer_row_insert_chars(moved, 0, indent, indent_length);
  } else if (closer) {
    const BufferRow* opener = indent_find_opener(language, moved, 0);
    if (opener != NULL) {
      indent_length = 0;
      editor_indent_append(indent, &indent_length, opener->data,
                           indent_leading(opener->data, opener->len));
    }
    buffer_row_insert_chars(moved, 0, indent, indent_length);
  } else {
    buffer_row_insert_chars(moved, 0, indent, indent_length);
  }
  editor_note_change(editor);
  g_autoindent_row = indent_length > 0 ? moved : NULL;
  editor_cursor_to_next_line(editor, indent_length);
}

// O: a new line above the cursor's, indented like it (a step more when that
// line starts with a closing bracket).
static void editor_open_line_above(Editor* editor) {
  Buffer* buffer = editor->current_buffer;
  BufferRow* row = buffer_get_current_line(buffer);
  if (row == NULL) {
    return;
  }
  char indent[EDITOR_INDENT_MAX];
  int indent_length = 0;
  if (editor->config.autoindent) {
    editor_indent_append(indent, &indent_length, row->data,
                         indent_leading(row->data, row->len));
    if (editor_indent_language(editor) != IndentLanguage_None &&
        indent_starts_with_closer(row->data, row->len)) {
      char unit[EDITOR_INDENT_MAX];
      const int unit_length = editor_indent_unit(editor, unit);
      editor_indent_append(indent, &indent_length, unit, unit_length);
    }
  }
  editor_mark_dirty_from_cursor(editor);
  buffer_break_current_line(buffer, 0);  // the line moves down, row is empty
  buffer_row_insert_chars(row, 0, indent, indent_length);
  editor_note_change(editor);
  g_autoindent_row = indent_length > 0 ? row : NULL;
  editor_set_cursor_column(editor, indent_length);
}

// Esc: a line that holds only the indentation Enter gave it becomes empty.
static void editor_indent_leave_insert(Editor* editor) {
  BufferRow* row = buffer_get_current_line(editor->current_buffer);
  if (row != NULL && row == g_autoindent_row && row->len > 0 &&
      indent_leading(row->data, row->len) == row->len) {
    buffer_row_trim(row, 0);
    editor_note_change(editor);
  }
  g_autoindent_row = NULL;
}

void editor_insert_char(Editor* editor, int key) {
  BufferRow* current_row = buffer_get_current_line(editor->current_buffer);
  switch (key) {
    case KEY_LEFT: {
      // Move cursor left
      editor->end_line_mode = false;
      editor_move_cursor_x(editor, -1, true);
      return;
    }
    case KEY_RIGHT: {
      // Move cursor right
      editor_move_cursor_x(editor, 1, true);
      return;
    }
    case KEY_UP: {
      // Move cursor up
      if (buffer_current_is_first_row(editor->current_buffer)) {
        return;
      }
      editor_move_cursor_y(editor, -1);
      buffer_scroll_rows(editor->current_buffer, -1);
      editor_fix_cursor_position(editor);
      return;
    }
    case KEY_DOWN: {
      // Move cursor down
      if (buffer_current_is_last_row(editor->current_buffer)) {
        return;
      }
      editor_move_cursor_y(editor, 1);
      buffer_scroll_rows(editor->current_buffer, 1);
      editor_fix_cursor_position(editor);
      return;
    }
    case KEY_BACKSPACE:
    case 127:
    case EDITOR_KEY_CTRL_H: {  // Ctrl-H is Backspace in insert mode, as in vim
      g_autoindent_row = NULL;
      const int x = editor_get_cursor_x(editor);
      if (x > 0) {
        // In the indentation, back to the previous indent step.
        const int count = editor_indent_backspace_count(editor, current_row, x);
        if (buffer_row_remove_chars(current_row, x - count, count) > 0) {
          editor_note_change(editor);
        }
        editor_set_cursor_column(editor, x - count);
      } else if (current_row != NULL && current_row->prev != NULL) {
        // At the start of a line: join it to the end of the line above.
        const int join_at = current_row->prev->len;
        if (buffer_join_current_line_with_previous(editor->current_buffer) > 0) {
          editor_note_change(editor);
          editor_move_cursor_y(editor, -1);
          editor_set_cursor_column(editor, join_at);
          editor_mark_dirty_from_cursor(editor);  // every line below moved up
        }
      }
      return;
    }
    case '\n': {
      editor_insert_newline(editor);
      return;
    }
    case '\t': {
      // Spaces: tab_size of them, or to the next indent step in the
      // indentation when autoindent is on.
      g_autoindent_row = NULL;
      const int x = editor_get_cursor_x(editor);
      const int count = editor_indent_tab_count(editor, current_row, x);
      for (int i = 0; i < count; ++i) {
        buffer_row_insert_char(current_row, x, ' ');
      }
      editor_note_change(editor);
      editor_set_cursor_column(editor, x + count);
      return;
    }
    default: {
      g_autoindent_row = NULL;
      const int x = editor_get_cursor_x(editor);
      buffer_row_insert_char(current_row, x, (char)key);
      editor_note_change(editor);
      editor_set_cursor_column(editor, editor_indent_closer(editor, current_row, x));
    }
  };
}

// The tree's keys. h goes up: closes an open directory, else moves to the
// directory above, else makes the root's parent the root.
static void editor_process_file_manager_key(Editor* editor, int key) {
  FileManager* fm = editor->file_manager;
  switch (key) {
    case 27:  // Escape: back to the text, the tree stays
      editor->state = EditorState_Running;
      break;
    case 'q':  // close the tree
      editor_toggle_file_manager(editor);
      editor->state = EditorState_Running;
      break;
    case 'j':
    case KEY_DOWN:
      file_manager_move_cursor_down(fm);
      break;
    case 'k':
    case KEY_UP:
      file_manager_move_cursor_up(fm);
      break;
    case 'h':
    case KEY_LEFT: {
      FileManagerEntry* entry = fm->cursor;
      if (entry != NULL && entry != fm->root &&
          entry->type == FileManagerEntryType_Directory && entry->expanded) {
        file_manager_toggle_expand(fm);
      } else if (entry != NULL && entry->parent != NULL && entry->parent != fm->root) {
        file_manager_select_path(fm, entry->parent->full_path);
      } else {
        file_manager_go_to_parent(fm);
      }
    } break;
    case 'l':
    case KEY_RIGHT:
    case '\n':  // Enter
    case 13:
      editor_file_manager_select(editor);
      break;
    case ' ':
    case 'o':
      file_manager_toggle_expand(fm);
      break;
    case 'C':  // the selected directory becomes the root
      file_manager_enter_directory(fm);
      break;
    case 'R':
      file_manager_refresh(fm);
      editor_set_status_message(editor, "Tree read again");
      break;
    case 'a':
      editor_file_manager_start_prompt(editor, FileManagerPrompt_NewFile);
      break;
    case 'A':
      editor_file_manager_start_prompt(editor, FileManagerPrompt_NewDirectory);
      break;
    case 'r':
    case 'm':
      editor_file_manager_start_prompt(editor, FileManagerPrompt_Rename);
      break;
    case 'c':
      editor_file_manager_start_prompt(editor, FileManagerPrompt_Copy);
      break;
    case 'd':
    case 'D':
      editor_file_manager_start_prompt(editor, FileManagerPrompt_Delete);
      break;
    case 2:  // Ctrl+B
      editor_toggle_file_manager(editor);
      editor->state = EditorState_Running;
      break;
  }
}

// ============================================================================
// Recording (config.record): every key typed in a file goes to FILE.rec
// ============================================================================

// The recording's name for a curses key, or NULL. The codes depend on the
// curses library (yasos_curses, ncurses, the tests' mock), so they are named
// here, where its header is included, rather than in record.c.
static const char* editor_curses_key_name(int key) {
  switch (key) {
    case KEY_UP:
      return "UP";
    case KEY_DOWN:
      return "DOWN";
    case KEY_LEFT:
      return "LEFT";
    case KEY_RIGHT:
      return "RIGHT";
    case KEY_HOME:
      return "HOME";
    case KEY_END:
      return "END";
    case KEY_PPAGE:
      return "PGUP";
    case KEY_NPAGE:
      return "PGDN";
    case KEY_DC:
      return "DEL";
    case KEY_BACKSPACE:
      return "KBS";
    default:
      return NULL;
  }
}

static void editor_record_stop(Editor* editor) {
  if (editor->recorder != NULL) {
    recorder_close(editor->recorder);
    free(editor->recorder);
    editor->recorder = NULL;
  }
}

// Before vi hands the terminal over or execs: the keys so far are written.
static void editor_record_flush(Editor* editor) {
  if (editor->recorder != NULL) {
    recorder_flush(editor->recorder);
  }
}

// The key goes to the recording of the buffer it was typed in, before it is
// processed (a key that switches buffers belongs to the one it left).
// The recorder, allocated on first use; NULL when out of memory.
static Recorder* editor_recorder(Editor* editor) {
  if (editor->recorder == NULL) {
    editor->recorder = (Recorder*)malloc(sizeof(Recorder));
    if (editor->recorder == NULL) {
      return NULL;
    }
    recorder_init(editor->recorder);
  }
  return editor->recorder;
}

// Whether keys typed in the current buffer are recorded.
static bool editor_record_wanted(const Editor* editor) {
  const Buffer* buffer = editor->current_buffer;
  return buffer != NULL && !buffer->autosave_disabled && record_wanted(buffer->filename);
}

// A "WORD AT_MS TEXT" line of the :term panel (record.h): to the open
// recording, else the current file's.
static void editor_record_note(Editor* editor, const char* word, const char* text) {
  if (!editor->config.record || editor_recorder(editor) == NULL) {
    return;
  }
  const Buffer* buffer = editor->current_buffer;
  const bool wanted = editor_record_wanted(editor);
  if (!recorder_note(editor->recorder, wanted ? buffer->filename : NULL,
                     wanted ? buffer_get_number_of_lines(buffer) : 0, word, text,
                     autosave_now_ms())) {
    char message[160];
    snprintf(message, sizeof(message), "Cannot record to %s" RECORD_SUFFIX,
             buffer->filename);
    editor_set_status_message(editor, message);
  }
}

// TermPanel.on_line: the panel's scrollback lines go to the recording.
static void editor_record_terminal_line(void* context,
                                        TermPanelLineKind kind,
                                        const char* text) {
  Editor* editor = (Editor*)context;
  switch (kind) {
    case TermPanelLine_Output:
      editor_record_note(editor, "out", text);
      break;
    case TermPanelLine_Command: {
      char line[TERMPANEL_INPUT_MAX + 8];
      snprintf(line, sizeof(line), "$ %s", text);
      editor_record_note(editor, "term", line);
    } break;
    case TermPanelLine_Exit: {
      char line[32];
      snprintf(line, sizeof(line), "exit %s", text);
      editor_record_note(editor, "term", line);
    } break;
  }
}

static bool editor_terminal_focused(const Editor* editor) {
  return editor->terminal != NULL && editor->terminal->visible &&
         editor->terminal->focused;
}

static void editor_record_key(Editor* editor, int key) {
  const Buffer* buffer = editor->current_buffer;
  if (!editor->config.record || !editor_record_wanted(editor) ||
      editor_recorder(editor) == NULL) {
    return;
  }
  char name[16];
  record_key_name(key, editor_curses_key_name(key), name, sizeof(name));
  if (!recorder_key(editor->recorder, buffer->filename,
                    buffer_get_number_of_lines(buffer), key, name,
                    autosave_now_ms())) {
    char message[160];
    snprintf(message, sizeof(message), "Cannot record to %s" RECORD_SUFFIX,
             buffer->filename);
    editor_set_status_message(editor, message);
  }
}

// ============================================================================
// :term -- a shell in a panel under the text (termpanel.h)
// ============================================================================

// Keeps the cursor inside the text when the panel takes rows from it.
static void editor_fit_cursor_to_text(Editor* editor) {
  const int last = editor->window.height - editor_bottom_height(editor) - 1;
  if (editor->cursor.y > last && last >= EDITOR_TOP_BAR_HEIGHT) {
    editor->start_line += editor->cursor.y - last;
    editor->cursor.y = last;
  }
  editor_mark_dirty_whole_screen(editor);
}

// The panel's height for this window: a third of it, at least 5 rows, and
// never so much that fewer than 3 rows of text are left.
static int editor_terminal_rows(const Editor* editor) {
  int rows = editor->window.height / 3;
  if (rows < 5) {
    rows = 5;
  }
  const int room = editor->window.height - EDITOR_TOP_BAR_HEIGHT -
                   EDITOR_TOOLBAR_HEIGHT - 3;
  if (rows > room) {
    rows = room;
  }
  return rows < 3 ? 3 : rows;
}

static TermPanel* editor_terminal(Editor* editor) {
  if (editor->terminal == NULL) {
    TermPanel* panel = (TermPanel*)malloc(sizeof(TermPanel));
    if (panel == NULL) {
      return NULL;
    }
    termpanel_init(panel);
    panel->on_line = editor_record_terminal_line;
    panel->on_line_context = editor;
    editor->terminal = panel;
  }
  return editor->terminal;
}

static void editor_terminal_show(Editor* editor, bool focus) {
  TermPanel* panel = editor->terminal;
  if (!panel->visible) {
    panel->rows = editor_terminal_rows(editor);
    panel->visible = true;
    editor_fit_cursor_to_text(editor);
  }
  panel->focused = focus;
}

static void editor_terminal_close(Editor* editor) {
  TermPanel* panel = editor->terminal;
  if (panel == NULL) {
    return;
  }
  termpanel_deinit(panel);
  free(panel);
  editor->terminal = NULL;
  editor_mark_dirty_whole_screen(editor);
}

// :term [CMD] -- shows the panel (starting the shell) and focuses it; CMD,
// with % for the current file, is sent to the shell.
static CommandResult editor_term(Editor* editor, const char* args) {
  TermPanel* panel = editor_terminal(editor);
  if (panel == NULL) {
    editor_set_error_message(editor, "Out of memory");
    return CommandResult_Error;
  }
  if (!termpanel_start(panel)) {
    editor_set_error_message(editor, "Cannot start " MAKE_SHELL);
    return CommandResult_Error;
  }
  editor_terminal_show(editor, true);
  if (args[0] != '\0') {
    char* expanded = editor_expand_command(editor, args);
    if (expanded == NULL) {
      return CommandResult_Error;
    }
    termpanel_send_line(panel, expanded);
    free(expanded);
  }
  return CommandResult_Success;
}

// Ctrl-W then KEY, from the text or the panel: j/Down the panel, k/Up the
// text, w/Ctrl-W the other one, c closes the panel (and its shell).
static void editor_window_command(Editor* editor, int key) {
  TermPanel* panel = editor->terminal;
  if (panel == NULL || !panel->visible) {
    return;
  }
  switch (key) {
    case 'j':
    case KEY_DOWN:
      panel->focused = true;
      break;
    case 'k':
    case KEY_UP:
      panel->focused = false;
      break;
    case 'w':
    case EDITOR_KEY_CTRL_W:
      panel->focused = !panel->focused;
      break;
    case 'c':
      editor_terminal_close(editor);
      break;
    default:
      break;
  }
}

// A key while the panel has the focus.
static void editor_terminal_key(Editor* editor, int key) {
  TermPanel* panel = editor->terminal;
  const int page = panel->rows > 3 ? panel->rows - 2 : 1;
  switch (key) {
    case '\n':
    case '\r':
#ifdef KEY_ENTER
    case KEY_ENTER:
#endif
      if (!termpanel_running(panel) && !termpanel_start(panel)) {
        editor_set_error_message(editor, "Cannot start " MAKE_SHELL);
        return;
      }
      termpanel_submit(panel);
      break;
    case 27:
    case EDITOR_KEY_CTRL_K:
      panel->focused = false;
      break;
    case EDITOR_KEY_CTRL_C:
      termpanel_input_clear(panel);
      termpanel_interrupt(panel);
      break;
    case EDITOR_KEY_CTRL_D:
      // End of input for the shell (exit), on an empty line as in a terminal.
      if (panel->input_length == 0 && panel->to_shell >= 0) {
        close(panel->to_shell);
        panel->to_shell = -1;
      }
      break;
    case EDITOR_KEY_CTRL_U:
      termpanel_input_clear(panel);
      break;
    case 127:
    case 8:
    case KEY_BACKSPACE:
      termpanel_input_backspace(panel);
      break;
    case KEY_UP:
      termpanel_history_step(panel, -1);
      break;
    case KEY_DOWN:
      termpanel_history_step(panel, 1);
      break;
    case KEY_PPAGE:
      panel->scroll += page;
      if (panel->scroll > termpanel_line_count(panel)) {
        panel->scroll = termpanel_line_count(panel);
      }
      break;
    case KEY_NPAGE:
      panel->scroll = panel->scroll > page ? panel->scroll - page : 0;
      break;
    default:
      if (key >= 32 && key < 127) {
        termpanel_input_char(panel, (char)key);
        panel->scroll = 0;
      } else if (key == '\t') {
        termpanel_input_char(panel, '\t');
      }
      break;
  }
}

// Ctrl-W and the panel's own keys. True when the key was taken.
static bool editor_terminal_handles_key(Editor* editor, int key) {
  static bool window_prefix;  // Ctrl-W came, its second key is next
  TermPanel* panel = editor->terminal;
  if (window_prefix) {
    window_prefix = false;
    editor_window_command(editor, key);
    return true;
  }
  if (panel == NULL || !panel->visible) {
    return false;
  }
  const bool editing_text = editor->state == EditorState_EditMode ||
                            editor->state == EditorState_CollectingCommand ||
                            editor->state == EditorState_SearchInputForward ||
                            editor->state == EditorState_SearchInputBackward;
  if (key == EDITOR_KEY_CTRL_W && (panel->focused || !editing_text)) {
    window_prefix = true;
    return true;
  }
  if (!panel->focused) {
    // Ctrl-J, window down, like Ctrl-H/L: the terminal sends it as Enter
    // (yasos_curses makes CR and LF both '\n'), which does nothing else in
    // normal mode.
    if (key == '\n' && editor->state == EditorState_Running &&
        editor->key_sequence[0] == '\0' && editor->repeat_count == 0) {
      panel->focused = true;
      return true;
    }
    return false;
  }
  editor_terminal_key(editor, key);
  return true;
}

// Text of a scrollback line for drawing: the partial line counts as the
// newest line while output is still arriving.
static const char* editor_terminal_row_text(const TermPanel* panel, int index,
                                            int total) {
  if (panel->partial_length > 0 && index == total - 1) {
    return NULL;  // the caller draws panel->partial
  }
  return termpanel_line(panel, index);
}

static void editor_draw_terminal(Editor* editor) {
  TermPanel* panel = editor->terminal;
  if (panel == NULL || !panel->visible) {
    return;
  }
  const int width = editor->window.width;
  const int top = editor->window.height - EDITOR_TOOLBAR_HEIGHT - panel->rows;
  const int output_rows = panel->rows - 2;

  // Title
  char title[160];
  if (termpanel_running(panel)) {
    snprintf(title, sizeof(title), " term: %s (pid %d)%s", MAKE_SHELL, (int)panel->pid,
             panel->focused ? "  Ctrl-C interrupt  Esc/Ctrl-W k text  Ctrl-W c close"
                            : "  Ctrl-W j to type here");
  } else {
    snprintf(title, sizeof(title), " term: shell exited %d  Enter starts one",
             panel->exit_status);
  }
  attrset(COLOR_OTHER | A_REVERSE);
  move(top, 0);
  for (int x = 0; x < width; ++x) {
    addch(' ');
  }
  mvaddnstr(top, 0, title, width);
  if (panel->scroll > 0) {
    char mark[24];
    snprintf(mark, sizeof(mark), " [+%d] ", panel->scroll);
    const int length = (int)strlen(mark);
    if (length < width) {
      mvaddstr(top, width - length, mark);
    }
  }
  attrset(COLOR_OTHER);

  // Output, newest at the bottom, long lines wrapped. Rows are filled from
  // the bottom up.
  const int total = termpanel_line_count(panel) + (panel->partial_length > 0 ? 1 : 0);
  int index = total - 1 - panel->scroll;
  int row = top + output_rows;  // the last output row
  while (row > top && index >= 0) {
    const char* text = editor_terminal_row_text(panel, index, total);
    int length;
    if (text == NULL) {
      text = panel->partial;
      length = (int)panel->partial_length;
    } else {
      length = (int)strlen(text);
    }
    const int pieces = width > 0 && length > 0 ? (length + width - 1) / width : 1;
    for (int piece = pieces - 1; piece >= 0 && row > top; --piece) {
      move(row, 0);
      const int start = piece * width;
      const int n = length - start < width ? length - start : width;
      if (n > 0) {
        mvaddnstr(row, 0, text + start, n);
      }
      clrtoeol();
      --row;
    }
    --index;
  }
  while (row > top) {
    move(row, 0);
    clrtoeol();
    --row;
  }

  // The line being typed: its end when it is longer than the row.
  const int input_row = top + panel->rows - 1;
  const int room = width > 3 ? width - 3 : 1;
  const int skip = (int)panel->input_length > room ? (int)panel->input_length - room : 0;
  move(input_row, 0);
  mvaddstr(input_row, 0, termpanel_running(panel) ? "$ " : "  ");
  mvaddstr(input_row, 2, panel->input + skip);
  clrtoeol();
}

// Where the cursor goes when the panel has the focus.
static void editor_terminal_place_cursor(Editor* editor) {
  const TermPanel* panel = editor->terminal;
  const int width = editor->window.width;
  const int room = width > 3 ? width - 3 : 1;
  const int shown = (int)panel->input_length > room ? room : (int)panel->input_length;
  move(editor->window.height - EDITOR_TOOLBAR_HEIGHT - 1, 2 + shown);
}

static void editor_handle_key(Editor* editor, int key);

void editor_process_key(Editor* editor, int key) {
  editor_record_key(editor, key);
  const bool terminal_focused = editor_terminal_focused(editor);
  editor_handle_key(editor, key);
  if (editor->config.record && editor_terminal_focused(editor) != terminal_focused) {
    // the key moved the focus between the text and the :term panel
    editor_record_note(editor, terminal_focused ? "text" : "term", "focus");
  }
}

static void editor_handle_key(Editor* editor, int key) {

  if (editor->state == EditorState_FileManager && editor->file_manager != NULL &&
      editor->file_manager->prompt != FileManagerPrompt_None) {
    editor_file_manager_prompt_key(editor, key);
    return;
  }

  if (editor_terminal_handles_key(editor, key)) {
    return;
  }

  // Update debug keystroke widget if available
  if (editor->toolbar) {
    Widget* debug_widget = toolbar_find_widget(editor->toolbar, "debug_keystroke");
    if (debug_widget) {
      widget_debug_keystroke_add_key(debug_widget, key);
    }
  }

  // Handle Ctrl+B to toggle file manager from any state except editing
  if (key == 2 && editor->state != EditorState_EditMode) {
    if (editor->file_manager && editor->file_manager->visible) {
      // If file manager is visible, toggle it off and return to running
      editor_toggle_file_manager(editor);
      editor->state = EditorState_Running;
    } else {
      // Toggle on and switch to file manager state
      editor_toggle_file_manager(editor);
      editor->state = EditorState_FileManager;
    }
    return;
  }

  // Ctrl-H and Ctrl-L move between the file manager window (left) and the
  // editor window (right), like vim's Ctrl-W h/l. Ctrl-J (Enter) and Ctrl-K
  // move between the text and the :term panel under it
  // (editor_terminal_handles_key). Only from normal mode and the tree: while
  // typing text, a : command or a search, Ctrl-H is Backspace.
  if (editor->state == EditorState_Running || editor->state == EditorState_FileManager) {
    if (key == EDITOR_KEY_CTRL_H && editor->file_manager &&
        editor->state != EditorState_FileManager) {
      // Ctrl-H: focus the file manager window, opening it when closed
      if (!editor->file_manager->visible) {
        editor_toggle_file_manager(editor);  // reads the tree again
      } else {
        file_manager_refresh(editor->file_manager);
      }
      editor->state = EditorState_FileManager;
      return;
    }
    if (key == EDITOR_KEY_CTRL_L) {
      // Ctrl-L: back to the editor window, the tree stays shown; there is
      // no window to the editor's right, so from there it does nothing.
      if (editor->state == EditorState_FileManager) {
        editor->state = EditorState_Running;
      }
      return;
    }
  }

  bool done = false;
  editor->key = key;
  while (!done) {
    switch (editor->state) {
      case EditorState_CollectingCommand: {
        if (!editor_collect_command(editor, key)) {
          return;
        }
      } break;
      case EditorState_ProcessingCommand: {
        int command_result = editor_process_command(editor);
        switch (command_result) {
          case CommandResult_ShouldExit: {
            editor->state = EditorState_Exiting;
          } break;
          case CommandResult_Error: {
            // Reason already on screen; leave it there.
            editor->state = EditorState_Running;
          } break;
          case CommandResult_Success: {
            // A command may hand the keys elsewhere (:e DIR to the tree).
            if (editor->state == EditorState_ProcessingCommand) {
              editor->state = EditorState_Running;
            }
            editor->command.buffer[0] = '\0';  // Clear command buffer
          } break;
          default: {
          }
        }
        command_deinit(&editor->command);
        return;
      } break;
      case EditorState_Running:
        if (editor->key_sequence[0] != 0) {
          if (editor_process_key_sequence(editor, key)) {
            return;
          }
        }
        switch (key) {
          case ':': {
            if (editor->error_message) {
              editor_clear_error_message(editor);
            }
            command_init(&editor->command);
            editor->state = EditorState_CollectingCommand;
            return;
          } break;
          default: {
            editor_restore_cursor_position(editor);
            for (int i = 0; i < editor->repeat_count + 1; ++i) {
              editor_process_editor_key(editor, key);
              move(editor->cursor.y, editor->cursor.x);
            }
            editor->repeat_count = 0;
            return;
          }
        }
        break;
      case EditorState_EditMode:
        if (key == EDITOR_KEY_CTRL_N || key == EDITOR_KEY_CTRL_P) {
          editor_complete(editor, key == EDITOR_KEY_CTRL_N ? 1 : -1);
          return;
        }
        if (editor->completion != NULL && editor->completion->active) {
          // Ctrl-E puts back what was typed; anything else keeps the match,
          // and a key other than Ctrl-Y then does what it always does.
          editor_completion_end(editor, key == EDITOR_KEY_CTRL_E);
          if (key == EDITOR_KEY_CTRL_E || key == EDITOR_KEY_CTRL_Y) {
            return;
          }
        }
        if (key == 27) {
          editor->state = EditorState_Running;
          editor_indent_leave_insert(editor);
          // just in case we are after last character while appeding/removing last
          editor_fix_cursor_position(editor);
          return;
        }
        editor_insert_char(editor, key);
        return;
      case EditorState_FileManager:
        editor_process_file_manager_key(editor, key);
        return;
      case EditorState_SearchInputForward:
      case EditorState_SearchInputBackward: {
        if (!editor_collect_search_pattern(editor, key)) {
          return;
        }
      } break;
      case EditorState_Exiting:
        return;
    };
  }
}

bool editor_should_exit(const Editor* editor) {
  return editor->state == EditorState_Exiting;
}

void editor_draw_status_bar(const Editor* editor) {
  if (editor->state == EditorState_CollectingCommand) {
    if (editor->command.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, ':');
      mvaddstr(editor->window.height - 1, 1, editor->command.buffer);
    }
  }
  if (editor->state == EditorState_SearchInputForward) {
    if (editor->search_buffer.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, '/');
      mvaddstr(editor->window.height - 1, 1, editor->search_buffer.buffer);
    } else {
      mvaddch(editor->window.height - 1, 0, '/');
    }
  }
  if (editor->state == EditorState_SearchInputBackward) {
    if (editor->search_buffer.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, '?');
      mvaddstr(editor->window.height - 1, 1, editor->search_buffer.buffer);
    } else {
      mvaddch(editor->window.height - 1, 0, '?');
    }
  }
  if (editor->error_message) {
    mvaddstr(editor->window.height - 1, 1, editor->error_message);
  }
  if (editor->status_bar) {
    mvaddstr(editor->window.height - 2, 0, editor->status_bar);
  }
  if (editor->key_sequence[0] != 0) {
    mvaddstr(editor->window.height - 1, editor->window.width - 10,
             editor->key_sequence);
  }
#ifdef DEBUG
  // The last key and its code, as the toolbar's debug keystroke widget shows
  // them in the widget path -- gated the same way, so a release build does not
  // paint a debug readout into the status bar on every keypress.
  mvprintw(editor->window.height - 1, editor->window.width - 30, "'%c'(%d) ",
           editor->key, editor->key);
#endif
}

void editor_redraw_screen(Editor* editor) {
  // clear();
  curs_set(0);
  editor_draw_tab_bar(editor);
  editor_draw_file_manager(editor);
  editor_draw_buffers(editor);
  editor_draw_terminal(editor);

  // Use widget-based toolbar if available
  if (editor->toolbar) {
    toolbar_update(editor->toolbar, editor);
    toolbar_draw(editor->toolbar, editor);
  } else {
    // Fallback to old status bar
    editor_draw_status_bar(editor);
  }

  // Draw search input line if in search mode (overrides toolbar/status bar)
  if (editor->state == EditorState_SearchInputForward) {
    if (editor->search_buffer.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, '/');
      mvaddstr(editor->window.height - 1, 1, editor->search_buffer.buffer);
    } else {
      mvaddch(editor->window.height - 1, 0, '/');
    }
  } else if (editor->state == EditorState_SearchInputBackward) {
    if (editor->search_buffer.buffer != NULL) {
      mvaddch(editor->window.height - 1, 0, '?');
      mvaddstr(editor->window.height - 1, 1, editor->search_buffer.buffer);
    } else {
      mvaddch(editor->window.height - 1, 0, '?');
    }
  }

  if (editor->state == EditorState_EditMode && editor->completion != NULL &&
      editor->completion->active) {
    editor_draw_completion_menu(editor);
  }

  if (editor->state == EditorState_FileManager && editor->file_manager != NULL &&
      editor->file_manager->prompt != FileManagerPrompt_None) {
    editor_draw_file_manager_prompt(editor);
  } else if (editor->terminal != NULL && editor->terminal->visible &&
             editor->terminal->focused) {
    editor_terminal_place_cursor(editor);
  } else {
    switch (editor->state) {
      case EditorState_Running:
      case EditorState_EditMode:
        editor_restore_cursor_position(editor);
        break;
      default:
        break;
    }
  }
  window_redraw_screen(&editor->window);
  curs_set(1);
}

void editor_init(Editor* editor) {
  window_init(&editor->window);

  // Create and initialize file manager
  editor->file_manager = file_manager_alloc();
  if (editor->file_manager) {
    file_manager_init(editor->file_manager, NULL);
    editor->file_manager->visible = false;
  }
  editor->editor_offset_x = 0;

  // Initialize search state
  search_init(&editor->search);
  command_init(&editor->search_buffer);

  // Create and initialize toolbar
  editor->toolbar = malloc(sizeof(struct Toolbar));
  if (editor->toolbar) {
    toolbar_init(editor->toolbar, editor->window.height, editor->window.width);

    // Add default widgets
    toolbar_add_widget(editor->toolbar, widget_mode_create());
    toolbar_add_widget(editor->toolbar, widget_filename_create());
    toolbar_add_widget(editor->toolbar, widget_position_create());
    toolbar_add_widget(editor->toolbar, widget_command_create());
    toolbar_add_widget(editor->toolbar, widget_message_create());
    toolbar_add_widget(editor->toolbar, widget_keyseq_create());

#ifdef DEBUG
    // Add debug keystroke widget in debug builds
    DebugKeystrokeConfig debug_config = {
        .show_ascii_char = true,
        .show_decimal_code = true,
        .show_hex_code = true,
        .show_octal_code = false,
        .compact_mode = true,
    };
    toolbar_add_widget(editor->toolbar, widget_debug_keystroke_create(&debug_config));
#endif

    toolbar_layout(editor->toolbar);
  }

  editor_home_cursor_xy(editor);
  editor->current_buffer_index = 0;
  move(editor->cursor.y, editor->cursor.x);
}

void editor_deinit(Editor* editor) {
  editor_record_stop(editor);
  editor_terminal_close(editor);
  if (editor->completion != NULL) {
    completion_reset(editor->completion);
    free(editor->completion);
    editor->completion = NULL;
  }
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    // A clean exit: what is unsaved was given up (as vim drops its .swp).
    autosave_remove(editor->buffers[i]);
    buffer_free(editor->buffers[i]);
  }
  free(editor->buffers);
  editor->buffers = NULL;
  editor->number_of_buffers = 0;
  editor->current_buffer = NULL;
  if (editor->error_message) {
    free(editor->error_message);
    editor->error_message = NULL;
  }
  // Clean up command buffer if it was initialized
  if (editor->command.buffer != NULL) {
    command_deinit(&editor->command);
  }
  // Clean up toolbar
  if (editor->toolbar) {
    toolbar_deinit(editor->toolbar);
    free(editor->toolbar);
    editor->toolbar = NULL;
  }
  // Clean up file manager
  if (editor->file_manager) {
    file_manager_free(editor->file_manager);
    editor->file_manager = NULL;
  }
  for (int i = 0; i < editor->tag_stack_size; ++i) {
    free(editor->tag_stack[i].filename);
  }
  editor->tag_stack_size = 0;
  editor_make_state_free(editor);
  // Clean up search state
  search_deinit(&editor->search);
  if (editor->search_buffer.buffer != NULL) {
    command_deinit(&editor->search_buffer);
  }
  window_deinit(&editor->window);
}

void editor_toggle_file_manager(Editor* editor) {
  if (!editor->file_manager) {
    return;
  }

  // Get current offset before toggle
  int old_offset = editor_get_content_offset_x(editor);

  file_manager_toggle_visibility(editor->file_manager);
  if (editor->file_manager->visible) {
    file_manager_refresh(editor->file_manager);  // files made since it was shown
  }

  // Get new offset after toggle
  int new_offset = editor_get_content_offset_x(editor);
  int offset_delta = new_offset - old_offset;
  
  // Update editor_offset_x immediately (also updated during draw)
  editor->editor_offset_x = new_offset;

  // Mark all rows dirty to force redraw
  editor_mark_dirty_whole_screen(editor);

  // Adjust cursor position by the offset change
  editor->number_of_line_digits = 4 + new_offset;
  editor->cursor.x += offset_delta;

  // Ensure cursor stays within valid bounds
  if (editor->cursor.x < editor->number_of_line_digits) {
    editor->cursor.x = editor->number_of_line_digits;
  }
}

// After loading a file: an autosave a crash left for it is announced, and
// waits for :recover or :delautosave (until then the buffer gets none).
static void editor_check_autosave(Editor* editor, Buffer* buffer) {
  if (autosave_check_stale(buffer)) {
    editor_set_error_message(editor,
                             "Autosave found: :recover restores, :delautosave drops");
  }
}

void editor_autosave_tick(Editor* editor, long long now_ms) {
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    Buffer* buffer = editor->buffers[i];
    if (buffer->autosave_disabled || buffer->stale_autosave != NULL ||
        buffer->changes == buffer->autosaved_changes) {
      continue;
    }
    if (buffer->changes != buffer->observed_changes) {
      buffer->observed_changes = buffer->changes;
      buffer->observed_at_ms = now_ms;
    }
    if (now_ms - buffer->observed_at_ms < AUTOSAVE_IDLE_MS &&
        buffer->changes - buffer->autosaved_changes < AUTOSAVE_MAX_PENDING) {
      continue;
    }
    if (!autosave_write(buffer)) {
      // Try again after the next edit, not on every pass of the main loop.
      buffer->autosaved_changes = buffer->changes;
      editor_set_error_message(editor, "Autosave failed: no writable directory");
    }
  }
}

bool editor_idle(Editor* editor) {
  bool redraw = false;
  if (editor->terminal != NULL && termpanel_poll(editor->terminal)) {
    redraw = editor->terminal->visible;
  }
  // Only read the clock when something waits for an autosave or a flush.
  long long now_ms = -1;
  if (editor->recorder != NULL && editor->recorder->pending) {
    now_ms = autosave_now_ms();
    recorder_idle(editor->recorder, now_ms);
  }
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    const Buffer* buffer = editor->buffers[i];
    if (!buffer->autosave_disabled && buffer->stale_autosave == NULL &&
        buffer->changes != buffer->autosaved_changes) {
      editor_autosave_tick(editor, now_ms >= 0 ? now_ms : autosave_now_ms());
      return redraw;
    }
  }
  return redraw;
}

// ============================================================================
// Opening files (:e, :open, the file tree) and the tree's file operations
// ============================================================================

// The buffer holding `path` (any spelling of it: relative, absolute, ~).
static Buffer* editor_find_buffer_by_filename(Editor* editor, const char* filename) {
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    const char* buf_filename = buffer_get_filename(editor->buffers[i]);
    if (buf_filename && path_same(buf_filename, filename)) {
      return editor->buffers[i];
    }
  }
  return NULL;
}

// The empty "[No Name]" buffer vi starts with when given no file: :e
// replaces it instead of opening a second tab next to it.
static bool editor_buffer_is_blank(const Buffer* buffer) {
  if (buffer == NULL || buffer_get_filename(buffer) != NULL || buffer_is_modified(buffer) ||
      buffer->autosave_disabled || buffer_get_number_of_lines(buffer) > 1) {
    return false;
  }
  const BufferRow* row = buffer_get_first_row(buffer);
  return row == NULL || row->data == NULL || row->data[0] == '\0';
}

// Shows a directory in the file tree, as its root, with the keys there.
static void editor_open_directory(Editor* editor, const char* path) {
  if (editor->file_manager == NULL || !file_manager_init(editor->file_manager, path)) {
    editor_set_status_message(editor, "Cannot open the directory");
    return;
  }
  if (!editor->file_manager->visible) {
    editor_toggle_file_manager(editor);
  }
  editor->state = EditorState_FileManager;
  editor_mark_dirty_whole_screen(editor);
}

// Opens `name` in a buffer and shows it: an open buffer of the same file is
// switched to, a missing file is a new buffer that :w creates, and a
// directory opens in the file tree. False on failure (the reason is shown).
static bool editor_edit_path(Editor* editor, const char* name) {
  char* path = path_expand_home(name);
  if (path == NULL) {
    editor_set_status_message(editor, "Out of memory");
    return false;
  }
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    const char* filename = buffer_get_filename(editor->buffers[i]);
    if (filename != NULL && path_same(filename, path)) {
      if (i != editor->current_buffer_index || editor->current_buffer != editor->buffers[i]) {
        editor_switch_to_buffer_by_index(editor, i);
      }
      char message[300];
      snprintf(message, sizeof(message), "\"%s\" is open", path_basename(path));
      editor_set_status_message(editor, message);
      free(path);
      return true;
    }
  }
  struct stat st;
  bool exists = stat(path, &st) == 0;
  if (exists && S_ISDIR(st.st_mode)) {
    editor_open_directory(editor, path);
    free(path);
    return true;
  }
  Buffer* buffer = buffer_alloc();
  if (buffer == NULL) {
    editor_set_status_message(editor, "Failed to allocate memory for buffer");
    free(path);
    return false;
  }
  buffer_load_from_file(buffer, path);
  size_t index;
  if (editor->number_of_buffers > 0 &&
      editor_buffer_is_blank(editor->buffers[editor->current_buffer_index])) {
    index = editor->current_buffer_index;
    autosave_remove(editor->buffers[index]);
    buffer_free(editor->buffers[index]);
    editor->buffers[index] = buffer;
  } else {
    if (!editor_append_buffer(editor, buffer)) {
      buffer_free(buffer);
      free(path);
      return false;
    }
    index = editor->number_of_buffers - 1;
  }
  editor_switch_to_buffer_by_index(editor, index);
  char message[300];
  if (exists) {
    snprintf(message, sizeof(message), "\"%s\" %d lines", path_basename(path),
             buffer_get_number_of_lines(buffer));
  } else {
    snprintf(message, sizeof(message), "\"%s\" [New]", path_basename(path));
  }
  editor_set_status_message(editor, message);
  editor_check_autosave(editor, buffer);
  free(path);
  return true;
}

// :e and :e! -- read the current file again; :e refuses to drop changes.
static bool editor_reload_current(Editor* editor, bool force) {
  Buffer* old = editor->current_buffer;
  const char* filename = old != NULL ? buffer_get_filename(old) : NULL;
  if (filename == NULL || old->autosave_disabled) {
    editor_set_status_message(editor, "No file to read again");
    return false;
  }
  if (buffer_is_modified(old) && !force) {
    editor_set_status_message(editor, "No write since last change (:e! drops it)");
    return false;
  }
  Buffer* buffer = buffer_alloc();
  if (buffer == NULL) {
    editor_set_status_message(editor, "Failed to allocate memory for buffer");
    return false;
  }
  buffer_load_from_file(buffer, filename);
  int line = buffer_get_current_line_number(old) - 1;
  autosave_remove(old);
  buffer_free(old);
  editor->buffers[editor->current_buffer_index] = buffer;
  editor_switch_to_buffer_by_index(editor, editor->current_buffer_index);
  if (line > 0 && line < buffer_get_number_of_lines(buffer)) {
    editor_goto_position(editor, line, 0);
  }
  char message[300];
  snprintf(message, sizeof(message), "\"%s\" read again, %d lines",
           path_basename(buffer_get_filename(buffer)), buffer_get_number_of_lines(buffer));
  editor_set_status_message(editor, message);
  return true;
}

// :e [FILE], :e! [FILE], :edit, :o FILE, :open FILE. True when the command
// was one of these.
static bool editor_process_open_command(Editor* editor, CommandResult* result) {
  const char* cmd = editor->command.buffer;
  static const char* const names[] = {"edit", "open", "e", "o"};
  size_t length = 0;
  bool is_open = false;
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
    size_t n = strlen(names[i]);
    if (strncmp(cmd, names[i], n) == 0 &&
        (cmd[n] == '\0' || cmd[n] == ' ' || cmd[n] == '!')) {
      length = n;
      is_open = names[i][0] == 'o';
      break;
    }
  }
  if (length == 0) {
    return false;
  }
  const char* arg = cmd + length;
  bool force = *arg == '!';
  if (force) {
    ++arg;
  }
  if (*arg != '\0' && *arg != ' ') {
    return false;
  }
  while (*arg == ' ') {
    ++arg;
  }
  char name[1024];
  snprintf(name, sizeof(name), "%s", arg);
  size_t name_length = strlen(name);
  while (name_length > 0 && name[name_length - 1] == ' ') {
    name[--name_length] = '\0';
  }
  bool ok;
  if (name_length == 0) {
    if (is_open) {
      editor_set_status_message(editor, "Usage: :open FILE");
      ok = false;
    } else {
      ok = editor_reload_current(editor, force);
    }
  } else if (force && editor->current_buffer != NULL &&
             buffer_get_filename(editor->current_buffer) != NULL &&
             path_same(buffer_get_filename(editor->current_buffer), name)) {
    ok = editor_reload_current(editor, true);
  } else {
    ok = editor_edit_path(editor, name);
  }
  *result = ok ? CommandResult_Success : CommandResult_Error;
  return true;
}

// Tab on the command line completes the file name after :e, :open, :r, :w
// and their kin: the common part of every match, then the matches listed.
static void editor_complete_command_path(Editor* editor) {
  Command* command = &editor->command;
  if (command->buffer == NULL) {
    return;
  }
  const char* space = strchr(command->buffer, ' ');
  if (space == NULL) {
    return;
  }
  static const char* const commands[] = {"e",  "e!", "edit", "edit!", "o",  "open",
                                         "r",  "read", "w",  "w!",   "wq", "x"};
  size_t command_length = (size_t)(space - command->buffer);
  bool known = false;
  for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
    if (strlen(commands[i]) == command_length &&
        strncmp(command->buffer, commands[i], command_length) == 0) {
      known = true;
      break;
    }
  }
  if (!known) {
    return;
  }
  size_t word_offset = (size_t)(strrchr(command->buffer, ' ') + 1 - command->buffer);
  char** matches = NULL;
  int count = path_complete(command->buffer + word_offset, &matches);
  if (count == 0) {
    editor_set_status_message(editor, "No match");
    return;
  }
  char* completed = count == 1 ? strdup(matches[0]) : path_common_prefix(matches, count);
  if (completed != NULL) {
    if (strlen(completed) > strlen(command->buffer + word_offset)) {
      command->cursor_position = (int)word_offset;
      command->buffer[word_offset] = '\0';
      for (const char* p = completed; *p != '\0'; ++p) {
        command_append(command, *p);
      }
      editor_clear_error_message(editor);
    } else if (count > 1) {
      // Nothing more in common: list the names.
      char list[512];
      size_t used = 0;
      list[0] = '\0';
      for (int i = 0; i < count && used + 4 < sizeof(list); ++i) {
        const char* shown = path_basename(matches[i]);
        size_t shown_length = strlen(shown);
        if (used + shown_length + 2 >= sizeof(list) - 4) {
          memcpy(list + used, "...", 4);
          used += 3;
          break;
        }
        memcpy(list + used, shown, shown_length);
        used += shown_length;
        list[used++] = ' ';
        list[used] = '\0';
      }
      editor_set_status_message(editor, list);
    }
    free(completed);
  }
  path_matches_free(matches, count);
}

// Open buffers follow a rename: a file renamed, or anything below a
// directory renamed, keeps its buffer under the new name. Its autosave moves
// with it (written again under the new name at the next idle moment).
static void editor_paths_renamed(Editor* editor, const char* from, const char* to) {
  char* old_root = path_absolute(from);
  char* new_root = path_absolute(to);
  if (old_root == NULL || new_root == NULL) {
    free(old_root);
    free(new_root);
    return;
  }
  size_t old_length = strlen(old_root);
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    Buffer* buffer = editor->buffers[i];
    if (buffer->filename == NULL || buffer->autosave_disabled) {
      continue;
    }
    char* current = path_absolute(buffer->filename);
    if (current != NULL && path_is_within(current, old_root)) {
      char* renamed = malloc(strlen(new_root) + strlen(current + old_length) + 1);
      if (renamed != NULL) {
        strcpy(renamed, new_root);
        strcat(renamed, current + old_length);
        bool had_autosave = buffer->autosave_path != NULL;
        autosave_remove(buffer);
        if (had_autosave) {
          buffer->autosaved_changes = buffer->changes - 1;
        }
        free(buffer->filename);
        buffer->filename = renamed;
      }
    }
    free(current);
  }
  free(old_root);
  free(new_root);
}

static bool editor_path_is_open(Editor* editor, const char* path) {
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    const char* filename = buffer_get_filename(editor->buffers[i]);
    if (filename != NULL && !editor->buffers[i]->autosave_disabled &&
        path_is_within(filename, path)) {
      return true;
    }
  }
  return false;
}

static void editor_tree_report(Editor* editor, const char* verb, const char* path) {
  char message[320];
  snprintf(message, sizeof(message), "%s %s", verb, path_basename(path));
  editor_set_status_message(editor, message);
}

// What the tree's prompt was answered with: "a" and "A" make the entry
// (with any missing directories above it; a new file is opened), "r" renames
// or moves it (an existing directory, or a name ending in '/', takes it
// inside), "c" copies it the same way.
static void editor_file_manager_prompt_done(Editor* editor) {
  FileManager* fm = editor->file_manager;
  char answer[FILE_MANAGER_PROMPT_MAX + 1];
  snprintf(answer, sizeof(answer), "%s", fm->prompt_input);
  int kind = fm->prompt;
  char* target = fm->prompt_target != NULL ? strdup(fm->prompt_target) : NULL;
  file_manager_prompt_end(fm);
  char* start = answer;
  while (*start == ' ') {
    ++start;
  }
  size_t length = strlen(start);
  while (length > 0 && start[length - 1] == ' ') {
    start[--length] = '\0';
  }
  if (length == 0 || target == NULL) {
    free(target);
    editor_redraw_screen(editor);
    return;
  }
  bool into = start[length - 1] == '/';
  while (length > 1 && start[length - 1] == '/') {
    start[--length] = '\0';
  }

  const char* error = NULL;
  char* path = NULL;
  if (kind == FileManagerPrompt_NewFile || kind == FileManagerPrompt_NewDirectory) {
    path = path_resolve(target, start);
    bool directory = kind == FileManagerPrompt_NewDirectory || into;
    if (path == NULL) {
      error = "Out of memory";
    } else if (directory) {
      error = fileops_make_directory(path);
    } else {
      char* parent = path_dirname(path);
      struct stat st;
      if (parent != NULL && stat(parent, &st) != 0) {
        error = fileops_make_directory(parent);
      }
      free(parent);
      if (error == NULL) {
        error = fileops_create_file(path);
      }
    }
    if (error == NULL) {
      file_manager_refresh(fm);
      file_manager_select_path(fm, path);
      if (directory) {
        editor_tree_report(editor, "Created", path);
      } else {
        editor_edit_path(editor, path);
        editor->state = EditorState_Running;  // straight to typing in it
        editor_tree_report(editor, "Created", path);
      }
    }
  } else if (kind == FileManagerPrompt_Rename || kind == FileManagerPrompt_Copy) {
    char* dir = path_dirname(target);
    path = dir != NULL ? path_resolve(dir, start) : NULL;
    free(dir);
    struct stat st;
    if (path != NULL && (into || (stat(path, &st) == 0 && S_ISDIR(st.st_mode)))) {
      char* inside = path_join(path, path_basename(target));
      free(path);
      path = inside;
    }
    if (path == NULL) {
      error = "Out of memory";
    } else if (kind == FileManagerPrompt_Rename) {
      error = fileops_rename(target, path);
      if (error == NULL) {
        editor_paths_renamed(editor, target, path);
      }
    } else {
      error = fileops_copy(target, path);
    }
    if (error == NULL) {
      file_manager_refresh(fm);
      file_manager_select_path(fm, path);
      editor_mark_dirty_whole_screen(editor);  // the tab bar shows new names
      editor_tree_report(editor, kind == FileManagerPrompt_Rename ? "Renamed to" : "Copied to",
                         path);
    }
  }
  if (error != NULL) {
    editor_set_status_message(editor, error);
  }
  free(path);
  free(target);
}

static void editor_file_manager_delete(Editor* editor) {
  FileManager* fm = editor->file_manager;
  char* target = fm->prompt_target != NULL ? strdup(fm->prompt_target) : NULL;
  file_manager_prompt_end(fm);
  if (target == NULL) {
    return;
  }
  // The cursor goes to the entry above, else below, else the directory.
  const FileManagerEntry* entry = fm->cursor;
  const FileManagerEntry* next_entry =
    entry == NULL ? NULL : (entry->prev != NULL ? entry->prev
                            : entry->next != NULL ? entry->next : entry->parent);
  char* after = next_entry != NULL ? strdup(next_entry->full_path) : NULL;
  bool open = editor_path_is_open(editor, target);
  const char* error = fileops_remove(target);
  if (error != NULL) {
    editor_set_status_message(editor, error);
  } else {
    file_manager_refresh(fm);
    if (after != NULL) {
      file_manager_select_path(fm, after);
    }
    char message[320];
    snprintf(message, sizeof(message), open ? "Deleted %s (still open: :w writes it back)"
                                            : "Deleted %s",
             path_basename(target));
    editor_set_status_message(editor, message);
  }
  free(after);
  free(target);
}

// Keys while the tree asks something on the command line.
static void editor_file_manager_prompt_key(Editor* editor, int key) {
  FileManager* fm = editor->file_manager;
  if (fm->prompt == FileManagerPrompt_Delete) {
    if (key == 'y' || key == 'Y') {
      editor_file_manager_delete(editor);
    } else {
      file_manager_prompt_end(fm);
      editor_set_status_message(editor, "Nothing deleted");
    }
    return;
  }
  if (key == 27) {
    file_manager_prompt_end(fm);
    editor_redraw_screen(editor);
    return;
  }
  if (key == '\n' || key == 13) {
    editor_file_manager_prompt_done(editor);
    return;
  }
  if (key == KEY_BACKSPACE || key == 127 || key == 8) {
    if (fm->prompt_length > 0) {
      fm->prompt_input[--fm->prompt_length] = '\0';
    }
    return;
  }
  if (key == EDITOR_KEY_CTRL_U) {
    fm->prompt_length = 0;
    fm->prompt_input[0] = '\0';
    return;
  }
  if (editor_is_printable_key(key) && fm->prompt_length < FILE_MANAGER_PROMPT_MAX) {
    fm->prompt_input[fm->prompt_length++] = (char)key;
    fm->prompt_input[fm->prompt_length] = '\0';
  }
}

// The prompt on the bottom line, with the cursor after what is typed.
static void editor_draw_file_manager_prompt(Editor* editor) {
  const FileManager* fm = editor->file_manager;
  char label[400];
  file_manager_prompt_label(fm, label, sizeof(label));
  const int row = editor->window.height - 1;
  const int width = editor->window.width;
  int label_length = (int)strlen(label);
  if (label_length > width - 20 && fm->prompt != FileManagerPrompt_Delete) {
    label_length = width - 20;  // room left to type
  }
  if (label_length > width - 1) {
    label_length = width - 1;
  }
  int room = width - 1 - label_length;
  int input_length = (int)fm->prompt_length;
  const char* input = fm->prompt_input;
  if (input_length > room) {
    input += input_length - room;  // the end of a long name stays in view
    input_length = room;
  }
  attrset(COLOR_OTHER | A_BOLD);
  mvaddnstr(row, 0, label, label_length);
  attrset(COLOR_OTHER);
  if (input_length > 0) {
    mvaddnstr(row, label_length, input, input_length);
  }
  clrtoeol();
  move(row, label_length + input_length);
}

static void editor_file_manager_start_prompt(Editor* editor, FileManagerPrompt kind) {
  FileManager* fm = editor->file_manager;
  const FileManagerEntry* entry = fm->cursor;
  if (kind == FileManagerPrompt_NewFile || kind == FileManagerPrompt_NewDirectory) {
    char* dir = file_manager_target_directory(fm);
    if (dir == NULL) {
      return;
    }
    editor_clear_error_message(editor);
    file_manager_prompt_start(fm, kind, dir, "");
    free(dir);
    return;
  }
  if (entry == NULL || entry == fm->root) {
    editor_set_status_message(editor, "Select a file or directory first");
    return;
  }
  editor_clear_error_message(editor);
  file_manager_prompt_start(fm, kind, entry->full_path,
                            kind == FileManagerPrompt_Delete ? "" : entry->name);
}

void editor_file_manager_select(Editor* editor) {
  if (!editor->file_manager || !editor->file_manager->cursor) {
    return;
  }
  FileManagerEntry* entry = file_manager_get_selected_entry(editor->file_manager);
  if (!entry) {
    return;
  }
  if (entry->type == FileManagerEntryType_Directory) {
    file_manager_toggle_expand(editor->file_manager);
    return;
  }
  // The tree stays open; the keys go to the file.
  if (editor_edit_path(editor, entry->full_path)) {
    editor->state = EditorState_Running;
  }
}

void editor_load_file(Editor* editor, const char* filename) {
  Buffer* buffer = buffer_alloc();
  if (buffer == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffer");
    return;
  }
  buffer_load_from_file(buffer, filename);
  if (!editor_append_buffer(editor, buffer)) {
    editor_set_error_message(editor, "Failed to append buffer");
    buffer_free(buffer);
    return;
  }
  if (editor->current_buffer == NULL) {
    editor->current_buffer = buffer;
    editor->current_buffer_index = 0;
  }
  editor_check_autosave(editor, buffer);
}

void editor_create_new_file(Editor* editor) {
  Buffer* buffer = buffer_alloc();
  if (buffer == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffer");
    return;
  }
  buffer_append_line(buffer, "\n");  // Start with an empty line
  if (!editor_append_buffer(editor, buffer)) {
    editor_set_error_message(editor, "Failed to append buffer");
    buffer_free(buffer);
    return;
  }
  if (editor->current_buffer == NULL) {
    editor->current_buffer = buffer;
    editor->current_buffer_index = 0;
  }
}

// ============================================================================
// Buffer/Tab Management
// ============================================================================

void editor_switch_to_buffer_by_index(Editor* editor, size_t index) {
  if (index >= editor->number_of_buffers) {
    editor_set_error_message(editor, "Invalid buffer index");
    return;
  }
  editor->current_buffer_index = index;
  editor->current_buffer = editor->buffers[index];
  buffer_scroll_to_top(editor->current_buffer);
  editor->start_line = 0;
  editor->start_column = 0;
  editor_home_cursor_xy(editor);
  editor_mark_dirty_whole_screen(editor);
}

void editor_switch_to_next_buffer(Editor* editor) {
  if (editor->number_of_buffers <= 1) {
    return;
  }
  size_t next_index = editor->current_buffer_index + 1;
  if (next_index >= editor->number_of_buffers) {
    next_index = 0;  // Wrap around to first buffer
  }
  editor_switch_to_buffer_by_index(editor, next_index);
}

void editor_switch_to_prev_buffer(Editor* editor) {
  if (editor->number_of_buffers <= 1) {
    return;
  }
  size_t prev_index;
  if (editor->current_buffer_index == 0) {
    prev_index = editor->number_of_buffers - 1;  // Wrap around to last buffer
  } else {
    prev_index = editor->current_buffer_index - 1;
  }
  editor_switch_to_buffer_by_index(editor, prev_index);
}

void editor_close_current_buffer(Editor* editor) {
  if (editor->number_of_buffers <= 1) {
    editor_set_error_message(editor, "Cannot close the last buffer");
    return;
  }

  // Free the current buffer
  autosave_remove(editor->buffers[editor->current_buffer_index]);
  buffer_free(editor->buffers[editor->current_buffer_index]);

  // Shift remaining buffers down
  for (size_t i = editor->current_buffer_index; i < editor->number_of_buffers - 1; i++) {
    editor->buffers[i] = editor->buffers[i + 1];
  }
  editor->number_of_buffers--;

  // Reallocate the buffers array
  Buffer** new_buffers = (Buffer**)realloc(
    editor->buffers, sizeof(Buffer*) * editor->number_of_buffers);
  if (new_buffers != NULL || editor->number_of_buffers == 0) {
    editor->buffers = new_buffers;
  }

  // Switch to the previous buffer, or keep current index if possible
  if (editor->current_buffer_index > 0) {
    editor->current_buffer_index--;
  }
  // Otherwise stay at 0 (the new first buffer after shifting)
  editor->current_buffer = editor->buffers[editor->current_buffer_index];
  buffer_scroll_to_top(editor->current_buffer);
  editor->start_line = 0;
  editor->start_column = 0;
  editor_home_cursor_xy(editor);
  editor_mark_dirty_whole_screen(editor);
}

void editor_draw_tab_bar(Editor* editor) {
  if (editor->number_of_buffers == 0) {
    return;
  }

  // Clear the tab bar area (row 0)
  attrset(COLOR_OTHER);
  move(0, 0);
  for (int i = 0; i < editor->window.width; i++) {
    addch(' ');
  }

  int x = 0;
  const int max_tab_width = 20;
  const int min_tab_width = 8;

  for (size_t i = 0; i < editor->number_of_buffers; i++) {
    if (x >= editor->window.width - min_tab_width) {
      // Not enough space for more tabs, show indicator
      attrset(COLOR_OTHER | A_BOLD);
      mvaddstr(0, editor->window.width - 3, "...");
      attrset(COLOR_OTHER);
      break;
    }

    const char* filename = buffer_get_filename(editor->buffers[i]);
    if (!filename) {
      filename = "[No Name]";
    }

    // Extract just the basename from the path
    const char* basename = filename;
    const char* last_slash = strrchr(filename, '/');
    if (last_slash) {
      basename = last_slash + 1;
    }

    // Calculate tab width
    int name_len = strlen(basename);
    bool is_modified = buffer_is_modified(editor->buffers[i]);
    int tab_width = name_len + (is_modified ? 2 : 0) + 3;  // name + [+] + brackets + space
    if (tab_width > max_tab_width) {
      tab_width = max_tab_width;
    }
    if (tab_width < min_tab_width) {
      tab_width = min_tab_width;
    }

    // Ensure we don't overflow
    if (x + tab_width > editor->window.width) {
      tab_width = editor->window.width - x;
      if (tab_width < 4) break;
    }

    // Draw tab with highlighting for current buffer
    if (i == editor->current_buffer_index) {
      attrset(COLOR_PAIR(4) | A_BOLD | A_REVERSE);  // Yellow background for active tab
    } else {
      attrset(COLOR_OTHER | A_DIM);
    }

    // Draw tab content
    char tab_content[32];
    int content_len = snprintf(tab_content, sizeof(tab_content), " %s%s ",
                               basename, is_modified ? "+" : "");
    if (content_len > tab_width - 1) {
      // Truncate and add ellipsis
      tab_content[tab_width - 2] = '\0';
      strncat(tab_content, " ", sizeof(tab_content) - strlen(tab_content) - 1);
    }

    mvaddnstr(0, x, tab_content, tab_width - 1);
    x += tab_width;

    // Draw separator between tabs
    if (i < editor->number_of_buffers - 1 && x < editor->window.width) {
      attrset(COLOR_OTHER | A_DIM);
      mvaddch(0, x - 1, '|');
    }
  }

  attrset(COLOR_OTHER);

  // Draw separator line (row 1)
  attrset(COLOR_OTHER | A_DIM);
  move(1, 0);
  for (int i = 0; i < editor->window.width; i++) {
    addch('-');
  }
  attrset(COLOR_OTHER);
}

// ============================================================================
// ctags navigation
// ============================================================================

static bool editor_is_identifier_char(char c) {
  return isalnum((unsigned char)c) || c == '_';
}

// The identifier the cursor is on, malloc'd; NULL when it is on anything else.
static char* editor_word_under_cursor(Editor* editor) {
  const BufferRow* row = buffer_get_current_line(editor->current_buffer);
  const int x = editor_get_cursor_x(editor);
  if (row == NULL || x < 0 || x >= row->len ||
      !editor_is_identifier_char(row->data[x])) {
    return NULL;
  }
  int start = x;
  int end = x;
  while (start > 0 && editor_is_identifier_char(row->data[start - 1])) {
    --start;
  }
  while (end < row->len && editor_is_identifier_char(row->data[end])) {
    ++end;
  }
  char* word = (char*)malloc((size_t)(end - start) + 1);
  if (word != NULL) {
    memcpy(word, &row->data[start], (size_t)(end - start));
    word[end - start] = '\0';
  }
  return word;
}

// Makes `path` the current buffer, loading it if no buffer has it open.
static bool editor_open_path(Editor* editor, const char* path) {
  const char* current = editor->current_buffer
                          ? buffer_get_filename(editor->current_buffer)
                          : NULL;
  if (current != NULL && strcmp(current, path) == 0) {
    return true;
  }
  Buffer* existing = editor_find_buffer_by_filename(editor, path);
  if (existing != NULL) {
    for (size_t i = 0; i < editor->number_of_buffers; ++i) {
      if (editor->buffers[i] == existing) {
        editor_switch_to_buffer_by_index(editor, i);
        return true;
      }
    }
  }
  // buffer_load_from_file turns a missing file into an empty new one; a tag
  // pointing at a file that is gone should say so instead.
  FILE* probe = fopen(path, "r");
  if (probe == NULL) {
    editor_set_error_message(editor, "Cannot open file from tags");
    return false;
  }
  fclose(probe);
  Buffer* buffer = buffer_alloc();
  if (buffer == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffer");
    return false;
  }
  buffer_load_from_file(buffer, path);
  if (!editor_append_buffer(editor, buffer)) {
    editor_set_error_message(editor, "Failed to append buffer");
    buffer_free(buffer);
    return false;
  }
  editor_switch_to_buffer_by_index(editor, editor->number_of_buffers - 1);
  editor_check_autosave(editor, buffer);
  return true;
}

static void editor_push_tag_position(Editor* editor) {
  const char* filename = editor->current_buffer
                           ? buffer_get_filename(editor->current_buffer)
                           : NULL;
  if (filename == NULL) {
    return;  // an unnamed buffer has nowhere to come back to
  }
  const size_t length = strlen(filename);
  char* copy = (char*)malloc(length + 1);
  if (copy == NULL) {
    return;
  }
  memcpy(copy, filename, length + 1);
  if (editor->tag_stack_size == EDITOR_TAG_STACK_DEPTH) {
    // Full: forget the oldest position, as vim does.
    free(editor->tag_stack[0].filename);
    memmove(&editor->tag_stack[0], &editor->tag_stack[1],
            sizeof(TagStackEntry) * (EDITOR_TAG_STACK_DEPTH - 1));
    --editor->tag_stack_size;
  }
  TagStackEntry* entry = &editor->tag_stack[editor->tag_stack_size++];
  entry->filename = copy;
  entry->line = buffer_get_current_line_number(editor->current_buffer) - 1;
  entry->column = editor_get_cursor_x(editor);
}

bool editor_jump_to_tag(Editor* editor, const char* name) {
  const char* current = editor->current_buffer
                          ? buffer_get_filename(editor->current_buffer)
                          : NULL;
  char* tags_path = tags_find_file(current);
  const char* system_tags = editor_system_tags(editor);
  if (tags_path == NULL && system_tags == NULL) {
    editor_set_error_message(editor, "No tags file (run ctags -R)");
    return false;
  }
  TagEntry* entries = NULL;
  int count = tags_path != NULL ? tags_lookup(tags_path, name, &entries) : 0;
  free(tags_path);
  if (count == 0 && system_tags != NULL) {
    tags_free_entries(entries, 0);
    entries = NULL;
    count = tags_lookup(system_tags, name, &entries);  // the C library's
  }
  if (count == 0) {
    editor_set_error_message(editor, "Tag not found");
    return false;
  }

  // A definition in this file wins: a static function shadows another
  // file's function of the same name.
  int pick = 0;
  for (int i = 0; current != NULL && i < count; ++i) {
    if (strcmp(entries[i].filename, current) == 0) {
      pick = i;
      break;
    }
  }

  editor_push_tag_position(editor);
  if (!editor_open_path(editor, entries[pick].filename)) {
    // Nothing moved, so the position just pushed is not a place to return to.
    if (editor->tag_stack_size > 0) {
      free(editor->tag_stack[--editor->tag_stack_size].filename);
    }
    tags_free_entries(entries, count);
    return false;
  }

  int line = tags_find_line(&entries[pick], editor->current_buffer);
  int column = 0;
  if (line >= 0) {
    // Land on the name itself, as vim does, when the line spells it out.
    const BufferRow* row = buffer_get_row(editor->current_buffer, line);
    const size_t name_len = strlen(name);
    for (int i = 0; row != NULL && i + (int)name_len <= row->len; ++i) {
      if (memcmp(&row->data[i], name, name_len) == 0 &&
          (i == 0 || !editor_is_identifier_char(row->data[i - 1])) &&
          (i + (int)name_len == row->len ||
           !editor_is_identifier_char(row->data[i + name_len]))) {
        column = i;
        break;
      }
    }
  }
  editor_goto_position(editor, line >= 0 ? line : 0, column);

  if (line < 0) {
    editor_set_error_message(editor, "Tag moved: pattern not found in file");
  } else if (count > 1) {
    char message[48];
    snprintf(message, sizeof(message), "tag %d of %d", pick + 1, count);
    editor_set_error_message(editor, message);
  }
  tags_free_entries(entries, count);
  return true;
}

bool editor_pop_tag(Editor* editor) {
  if (editor->tag_stack_size == 0) {
    editor_set_error_message(editor, "At bottom of tag stack");
    return false;
  }
  TagStackEntry entry = editor->tag_stack[--editor->tag_stack_size];
  bool opened = editor_open_path(editor, entry.filename);
  if (opened) {
    editor_goto_position(editor, entry.line, entry.column);
  }
  free(entry.filename);
  return opened;
}

// ============================================================================
// :man and K
// ============================================================================

// A message for the status bar as it is, without the ": 'command'" that
// editor_set_error_message appends while a command runs.
static void editor_set_status_message(Editor* editor, const char* message) {
  editor_clear_error_message(editor);
  const size_t length = strlen(message);
  editor->error_message = (char*)malloc(length + 1);
  if (editor->error_message != NULL) {
    memcpy(editor->error_message, message, length + 1);
  }
  editor_redraw_screen(editor);
}

static bool editor_switch_to_buffer(Editor* editor, const Buffer* buffer) {
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    if (editor->buffers[i] == buffer) {
      editor_switch_to_buffer_by_index(editor, i);
      return true;
    }
  }
  return false;
}

// Shows `path` in a buffer that is not autosaved (a man page, the build log).
// An open buffer of that file is switched to; with `reload` its text is read
// again first (the log changes with every build).
static bool editor_open_view(Editor* editor, const char* path, bool reload) {
  FILE* probe = fopen(path, "r");
  if (probe == NULL) {
    editor_set_status_message(editor, "Cannot read the file");
    return false;
  }
  fclose(probe);
  Buffer* existing = editor_find_buffer_by_filename(editor, path);
  if (existing != NULL && !reload) {
    return editor_switch_to_buffer(editor, existing);
  }
  Buffer* buffer = buffer_alloc();
  if (buffer == NULL) {
    editor_set_error_message(editor, "Failed to allocate memory for buffer");
    return false;
  }
  buffer_load_from_file(buffer, path);
  buffer->autosave_disabled = true;  // a view: nothing to recover
  if (existing != NULL) {
    for (size_t i = 0; i < editor->number_of_buffers; ++i) {
      if (editor->buffers[i] == existing) {
        editor->buffers[i] = buffer;
        if (editor->current_buffer == existing) {
          editor->current_buffer = buffer;
        }
        autosave_remove(existing);
        buffer_free(existing);
        editor_switch_to_buffer_by_index(editor, i);
        return true;
      }
    }
  }
  if (!editor_append_buffer(editor, buffer)) {
    buffer_free(buffer);
    return false;
  }
  editor_switch_to_buffer_by_index(editor, editor->number_of_buffers - 1);
  return true;
}

// :man NAME (NULL or "": the whatis index), from $MANPATH.
static bool editor_show_man(Editor* editor, const char* name) {
  const char* manpath = getenv("MANPATH");
  char message[96];
  if (name == NULL || name[0] == '\0') {
    char* whatis = man_find_whatis(manpath);
    if (whatis == NULL) {
      editor_set_status_message(editor, "No whatis index in MANPATH");
      return false;
    }
    const bool shown = editor_open_view(editor, whatis, false);
    free(whatis);
    return shown;
  }
  ManPage page;
  if (!man_locate(manpath, name, &page)) {
    snprintf(message, sizeof(message), "No manual entry for %s", name);
    editor_set_status_message(editor, message);
    return false;
  }
  if (page.offset < 0 && !man_is_gzipped(page.file)) {
    const bool shown = editor_open_view(editor, page.file, false);
    man_page_free(&page);
    return shown;
  }
  // A gzipped or packed page is read from a copy in /tmp, and the buffer is
  // named after that copy: the next :man of the page finds it open.
  char* copy = man_temp_path(MAN_TEMP_DIR, page.name);
  if (copy == NULL) {
    man_page_free(&page);
    editor_set_error_message(editor, "Out of memory");
    return false;
  }
  Buffer* existing = editor_find_buffer_by_filename(editor, copy);
  bool shown = false;
  if (existing != NULL) {
    shown = editor_switch_to_buffer(editor, existing);
  } else {
    // A packed page: xzcat unpacks its ~64 KB block, man_cut keeps the page.
    static const char* const zcats[] = {"/bin/zcat", "/usr/bin/zcat"};
    static const char* const xzcats[] = {"/bin/xzcat", "/usr/bin/xzcat"};
    const bool packed = page.offset >= 0;
    const char* const* tools = packed ? xzcats : zcats;
    int status = -1;
    for (size_t i = 0; i < 2; ++i) {
      status = run_to_file(tools[i], page.file, copy);
      if (status != 127 && status != -1) {
        break;  // ran (127: no such tool, try the next)
      }
    }
    if (status == 0 && (!packed || man_cut(copy, page.offset, page.length))) {
      shown = editor_open_view(editor, copy, false);
    } else {
      remove(copy);
      snprintf(message, sizeof(message), "Cannot unpack the page for %s (%s)", name,
               packed ? "xzcat" : "zcat");
      editor_set_status_message(editor, message);
    }
  }
  free(copy);
  man_page_free(&page);
  return shown;
}

static bool editor_is_man_word_char(char c) {
  return editor_is_identifier_char(c) || c == '.' || c == '/' || c == '-';
}

// The word under the cursor for K: an identifier, also taking '.', '/' and '-'
// so stdio.h and sys/stat.h are one word. Leading and trailing punctuation is
// dropped ("printf." at the end of a sentence). malloc'd, or NULL.
static char* editor_man_word_under_cursor(Editor* editor) {
  const BufferRow* row = buffer_get_current_line(editor->current_buffer);
  const int x = editor_get_cursor_x(editor);
  if (row == NULL || x < 0 || x >= row->len ||
      !editor_is_man_word_char(row->data[x])) {
    return NULL;
  }
  int start = x;
  int end = x;
  while (start > 0 && editor_is_man_word_char(row->data[start - 1])) {
    --start;
  }
  while (end < row->len && editor_is_man_word_char(row->data[end])) {
    ++end;
  }
  while (start < end && !editor_is_identifier_char(row->data[start])) {
    ++start;
  }
  while (end > start && !editor_is_identifier_char(row->data[end - 1])) {
    --end;
  }
  if (start == end) {
    return NULL;
  }
  char* word = (char*)malloc((size_t)(end - start) + 1);
  if (word != NULL) {
    memcpy(word, &row->data[start], (size_t)(end - start));
    word[end - start] = '\0';
  }
  return word;
}

// K: the man page of the word under the cursor. The wide word (stdio.h) is
// tried first; when it has no page the identifier alone (a in a.b) is.
static void editor_man_under_cursor(Editor* editor) {
  char* wide = editor_man_word_under_cursor(editor);
  char* narrow = editor_word_under_cursor(editor);
  const char* name = wide != NULL ? wide : narrow;
  if (name == NULL) {
    editor_set_status_message(editor, "No word under cursor");
    return;
  }
  if (wide != NULL && narrow != NULL && strcmp(wide, narrow) != 0) {
    char* page = man_find_page(getenv("MANPATH"), wide);
    if (page == NULL) {
      name = narrow;
    }
    free(page);
  }
  editor_show_man(editor, name);
  free(wide);
  free(narrow);
}

// ============================================================================
// :make, :set and the error list (:cn :cp :cc :cl)
// ============================================================================

static MakeState* editor_make_state(Editor* editor) {
  if (editor->make == NULL) {
    MakeState* state = (MakeState*)calloc(1, sizeof(MakeState));
    if (state == NULL) {
      return NULL;
    }
    make_config_init(&state->config);
    state->errors.current = -1;
    editor->make = state;
  }
  return editor->make;
}

static void editor_make_state_free(Editor* editor) {
  MakeState* state = editor->make;
  if (state == NULL) {
    return;
  }
  make_config_deinit(&state->config);
  make_error_list_clear(&state->errors);
  free(state->log_path);
  free(state->program_path);
  free(state);
  editor->make = NULL;
}

static char* editor_strdup(const char* text) {
  const size_t length = strlen(text) + 1;
  char* copy = (char*)malloc(length);
  if (copy != NULL) {
    memcpy(copy, text, length);
  }
  return copy;
}

void editor_set_program_path(Editor* editor, const char* path) {
  MakeState* state = editor_make_state(editor);
  if (state == NULL || path == NULL) {
    return;
  }
  free(state->program_path);
  state->program_path = editor_strdup(path);
}

bool editor_apply_setting(Editor* editor, const char* line) {
  const ConfigSetResult result = config_set(&editor->config, line);
  if (result != ConfigSet_UnknownKey) {
    return result != ConfigSet_BadValue;
  }
  MakeState* state = editor_make_state(editor);
  if (state == NULL) {
    return false;
  }
  const MakeSetResult make_result = make_config_set(&state->config, line);
  return make_result == MakeSet_Ok || make_result == MakeSet_Ignored;
}

// A config file being read: which line, and the first one not understood.
typedef struct {
  Editor* editor;
  int line_number;
  int first_bad_line;  // 0 while none
  char first_bad_text[48];
} EditorConfigRead;

static bool editor_apply_setting_line(void* context, const char* line) {
  EditorConfigRead* read = (EditorConfigRead*)context;
  ++read->line_number;
  if (editor_apply_setting(read->editor, line)) {
    return true;
  }
  if (read->first_bad_line == 0) {
    read->first_bad_line = read->line_number;
    size_t length = strcspn(line, "\r\n");
    if (length >= sizeof(read->first_bad_text)) {
      length = sizeof(read->first_bad_text) - 1;
    }
    memcpy(read->first_bad_text, line, length);
    read->first_bad_text[length] = '\0';
  }
  return false;
}

// Reads one config file; a file with lines it did not understand is named in
// the first redraw's message (called before any buffer exists, so it is set
// here rather than drawn).
static bool editor_read_config_file(Editor* editor, const char* path) {
  int bad_lines = 0;
  EditorConfigRead read = {.editor = editor};
  if (!config_read_lines(path, editor_apply_setting_line, &read, &bad_lines)) {
    return false;
  }
  if (bad_lines > 0 && editor->error_message == NULL) {
    // The first line it did not understand, and what the settings are
    char more[32] = "";
    if (bad_lines > 1) {
      snprintf(more, sizeof(more), " (+%d more)", bad_lines - 1);
    }
    // ~ for the home directory keeps it on an 80-column line
    const char* home = getenv("HOME");
    const size_t home_length = home != NULL ? strlen(home) : 0;
    const bool in_home = home_length > 1 && strncmp(path, home, home_length) == 0 &&
                         path[home_length] == '/';
    char message[256];
    snprintf(message, sizeof(message), "%s%s:%d: unknown \"%s\"%s (record, ai, makeprg, makemode)",
             in_home ? "~" : "", in_home ? path + home_length : path, read.first_bad_line,
             read.first_bad_text, more);
    editor->error_message = editor_strdup(message);
  }
  return true;
}

void editor_load_config(Editor* editor) {
  // The user's settings first, so a project's .yasvirc overrides them.
  char* path = config_user_path(getenv("XDG_CONFIG_HOME"), getenv("HOME"));
  if (path != NULL) {
    editor_read_config_file(editor, path);
    free(path);
  }
  if (editor_read_config_file(editor, MAKE_RC_NAME)) {
    return;
  }
  const char* home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    return;
  }
  path = (char*)malloc(strlen(home) + sizeof("/" MAKE_RC_NAME));
  if (path != NULL) {
    sprintf(path, "%s/%s", home, MAKE_RC_NAME);
    editor_read_config_file(editor, path);
    free(path);
  }
}

// Writes the buffer to its own file, as :w does.
static bool editor_write_buffer_file(Buffer* buffer) {
  FILE* file = fopen(buffer->filename, "w");
  if (file == NULL) {
    return false;
  }
  bool written = true;
  for (BufferRow* row = buffer_get_first_row(buffer); row != NULL; row = row->next) {
    if (row->data && fprintf(file, "%s\n", row->data) < 0) {
      written = false;
    }
  }
  if (fclose(file) != 0 || !written) {
    return false;
  }
  buffer_mark_saved(buffer);
  autosave_remove(buffer);
  return true;
}

// :make's autowrite: every modified buffer with a filename that is a file
// (not a view). The names written go to `written`, space-separated. False
// when one could not be written; its name is then in `failed`.
static bool editor_autowrite(Editor* editor,
                             char* written,
                             size_t written_size,
                             const char** failed) {
  written[0] = '\0';
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    Buffer* buffer = editor->buffers[i];
    if (buffer->filename == NULL || buffer->autosave_disabled ||
        !buffer_is_modified(buffer)) {
      continue;
    }
    if (!editor_write_buffer_file(buffer)) {
      *failed = buffer->filename;
      return false;
    }
    const size_t used = strlen(written);
    snprintf(written + used, written_size - used, "%s%s", used > 0 ? " " : "",
             buffer->filename);
  }
  return true;
}

// Back from the terminal: curses starts again and every row is drawn.
static void editor_resume_screen(Editor* editor) {
  window_init(&editor->window);
  editor_mark_dirty_whole_screen(editor);
}

static void editor_print_build_start(const char* written, const char* command_line) {
  if (written[0] != '\0') {
    printf("wrote %s\n", written);
  }
  printf("%s\n", command_line);
  fflush(stdout);
}

// makemode=wait: vi stays in waitpid (where the kernel can swap it out)
// while the build has the terminal.
static int editor_run_build_wait(Editor* editor,
                                 const char* written,
                                 const char* command_line,
                                 const char* script) {
  editor_record_flush(editor);
  endwin();
  editor_print_build_start(written, command_line);
  int status = run_shell_wait(script);
  if (status < 0) {
    printf("cannot run %s\n", MAKE_SHELL);
  } else {
    status = make_read_status(MAKE_LOG_PATH);  // the build's, not tee's
  }
  printf("\n[build exited %d] press Enter", status);
  fflush(stdout);
  run_wait_for_enter();
  editor_resume_screen(editor);
  return status;
}

// makemode=exec: the session goes to MAKE_SESSION_PATH and vi becomes the
// build; the script runs vi -S afterwards. Returns only on failure.
static void editor_run_build_exec(Editor* editor,
                                  MakeState* state,
                                  const char* written,
                                  const char* command_line,
                                  const char* program,
                                  const char* args) {
  MakeSession session = {NULL, 0, 0};
  for (size_t i = 0; i < editor->number_of_buffers; ++i) {
    Buffer* buffer = editor->buffers[i];
    if (buffer->filename == NULL ||
        strcmp(buffer->filename, HELP_BUFFER_NAME) == 0) {
      continue;
    }
    const bool is_current = buffer == editor->current_buffer;
    if (is_current) {
      session.current = session.count;
    }
    make_session_add(
      &session, buffer->filename, buffer_get_current_line_number(buffer) - 1,
      is_current ? editor_get_cursor_x(editor) : 0, buffer->autosave_disabled);
  }
  const bool saved = make_session_write(MAKE_SESSION_PATH, &session, &state->config);
  make_session_clear(&session);
  if (!saved) {
    editor_set_status_message(editor, "Cannot write " MAKE_SESSION_PATH);
    return;
  }
  const char* vi_path =
    state->program_path != NULL && strchr(state->program_path, '/') != NULL
      ? state->program_path
      : "vi";
  char* script =
    make_build_exec_script(program, args, MAKE_LOG_PATH, vi_path, MAKE_SESSION_PATH);
  if (script == NULL) {
    editor_set_error_message(editor, "Out of memory");
    return;
  }
  editor_record_flush(editor);  // exec drops stdio's buffers
  endwin();
  editor_print_build_start(written, command_line);
  remove(MAKE_LOG_PATH MAKE_RC_SUFFIX);
  run_shell_exec(script);
  free(script);
  remove(MAKE_SESSION_PATH);
  editor_resume_screen(editor);
  editor_set_status_message(editor, "Cannot exec " MAKE_SHELL);
}

static bool editor_goto_error(Editor* editor, int index) {
  MakeState* state = editor->make;
  if (state == NULL || index < 0 || index >= state->errors.count) {
    return false;
  }
  const MakeError* error = &state->errors.items[index];
  char message[256];
  FILE* probe = fopen(error->filename, "r");
  if (probe == NULL) {
    snprintf(message, sizeof(message), "Cannot open %s", error->filename);
    editor_set_status_message(editor, message);
    return false;
  }
  fclose(probe);
  if (!editor_open_path(editor, error->filename)) {
    return false;
  }
  state->errors.current = index;
  int line = error->line - 1;
  const int last = buffer_get_number_of_lines(editor->current_buffer) - 1;
  if (line > last) {
    line = last;
  }
  editor_goto_position(editor, line < 0 ? 0 : line,
                       error->column > 0 ? error->column - 1 : 0);
  const int used =
    snprintf(message, sizeof(message), "(%d of %d) ", index + 1, state->errors.count);
  make_error_format(error, message + used, sizeof(message) - (size_t)used);
  editor_set_status_message(editor, message);
  return true;
}

void editor_make_report(Editor* editor, const char* log_path, int status) {
  MakeState* state = editor_make_state(editor);
  if (state == NULL) {
    return;
  }
  free(state->log_path);
  state->log_path = editor_strdup(log_path);
  make_parse_log(log_path, &state->errors);
  const MakeErrorList* errors = &state->errors;
  const int warnings = errors->count - errors->errors;
  char summary[96];
  if (status == 0) {
    snprintf(summary, sizeof(summary), "build ok");
  } else if (status < 0) {
    snprintf(summary, sizeof(summary), "build failed (not started)");
  } else {
    snprintf(summary, sizeof(summary), "build failed (exit %d), %d error%s", status,
             errors->errors, errors->errors == 1 ? "" : "s");
  }
  if (warnings > 0) {
    const size_t used = strlen(summary);
    snprintf(summary + used, sizeof(summary) - used, ", %d warning%s", warnings,
             warnings == 1 ? "" : "s");
  }
  int first_error = -1;
  for (int i = 0; i < errors->count; ++i) {
    if (!errors->items[i].is_warning) {
      first_error = i;
      break;
    }
  }
  if (first_error < 0 || !editor_goto_error(editor, first_error)) {
    editor_set_status_message(editor, summary);
    return;
  }
  // The jump shows the error; the summary goes in front of it.
  char message[256];
  const int used = snprintf(message, sizeof(message), "%s: ", summary);
  make_error_format(&errors->items[first_error], message + used,
                    sizeof(message) - (size_t)used);
  editor_set_status_message(editor, message);
}

static CommandResult editor_make(Editor* editor, const char* args) {
  MakeState* state = editor_make_state(editor);
  if (state == NULL) {
    editor_set_error_message(editor, "Out of memory");
    return CommandResult_Error;
  }
  char* program = make_resolve_program(&state->config, NULL);
  if (program == NULL) {
    editor_set_status_message(editor,
                              "No build.sh or Makefile: :set makeprg=CMD "
                              "or makeprg=CMD in .yasvirc");
    return CommandResult_Error;
  }
  if (state->config.mode == MakeMode_Exec) {
    for (size_t i = 0; i < editor->number_of_buffers; ++i) {
      if (editor->buffers[i]->filename == NULL &&
          buffer_is_modified(editor->buffers[i])) {
        free(program);
        editor_set_status_message(editor,
                                  "Unnamed buffer modified: :w NAME first "
                                  "(makemode=exec restarts vi)");
        return CommandResult_Error;
      }
    }
  }
  char written[256];
  const char* failed = NULL;
  if (!editor_autowrite(editor, written, sizeof(written), &failed)) {
    char message[160];
    snprintf(message, sizeof(message), "Cannot write %s; build not started", failed);
    free(program);
    editor_set_status_message(editor, message);
    return CommandResult_Error;
  }
  char* script = make_build_script(program, args, MAKE_LOG_PATH);
  // What is printed before the build: the command as the user knows it.
  const size_t command_size = strlen(program) + strlen(args) + 2;
  char* command_line = (char*)malloc(command_size);
  if (script == NULL || command_line == NULL) {
    free(program);
    free(script);
    free(command_line);
    editor_set_error_message(editor, "Out of memory");
    return CommandResult_Error;
  }
  snprintf(command_line, command_size, "%s%s%s", program, args[0] ? " " : "", args);
  if (state->config.mode == MakeMode_Exec) {
    editor_run_build_exec(editor, state, written, command_line, program, args);
    free(program);
    free(script);
    free(command_line);
    return CommandResult_Error;  // still here: the exec failed
  }
  const int status = editor_run_build_wait(editor, written, command_line, script);
  free(program);
  free(script);
  free(command_line);
  editor_make_report(editor, MAKE_LOG_PATH, status);
  return CommandResult_Success;
}

// :set (show), :set key=value, :set key (show)
static CommandResult editor_set_option(Editor* editor, const char* args) {
  MakeState* state = editor_make_state(editor);
  if (state == NULL) {
    editor_set_error_message(editor, "Out of memory");
    return CommandResult_Error;
  }
  const ConfigSetResult config_result = config_set(&editor->config, args);
  if (config_result == ConfigSet_BadValue) {
    char name[64];
    char error[96];
    config_describe_setting(&editor->config, args, name, sizeof(name));
    // "noautoindent" or "autoindent": the setting's name either way
    snprintf(error, sizeof(error), "%s is on or off",
             strncmp(name, "no", 2) == 0 ? name + 2 : name);
    editor_set_error_message(editor, error);
    return CommandResult_Error;
  }
  if (!editor->config.record) {
    editor_record_stop(editor);
  }
  char options[64];
  if (config_result == ConfigSet_Ok || config_result == ConfigSet_Query) {
    config_describe_setting(&editor->config, args, options, sizeof(options));
    editor_set_status_message(editor, options);
    return CommandResult_Success;
  }
  config_describe(&editor->config, options, sizeof(options));
  switch (config_result == ConfigSet_UnknownKey ? make_config_set(&state->config, args)
                                                : MakeSet_Ignored) {
    case MakeSet_UnknownKey:
      editor_set_error_message(editor,
                               "Unknown option (record, autoindent, makeprg, makemode)");
      return CommandResult_Error;
    case MakeSet_BadValue:
      editor_set_error_message(editor, "makemode is wait or exec");
      return CommandResult_Error;
    case MakeSet_NoMemory:
      editor_set_error_message(editor, "Out of memory");
      return CommandResult_Error;
    default:
      break;
  }
  char description[192];
  make_config_describe(&state->config, NULL, description, sizeof(description));
  char message[260];
  snprintf(message, sizeof(message), "%s %s", options, description);
  editor_set_status_message(editor, message);
  return CommandResult_Success;
}

// ============================================================================
// :!cmd, :r !cmd and :r FILE
// ============================================================================

// CMD with % expanded to the current file (run_expand_command); NULL after
// saying why.
static char* editor_expand_command(Editor* editor, const char* command) {
  bool no_name = false;
  const char* filename =
    editor->current_buffer != NULL ? editor->current_buffer->filename : NULL;
  char* expanded = run_expand_command(command, filename, &no_name);
  if (expanded == NULL) {
    editor_set_error_message(editor, no_name ? "No file name for %" : "Out of memory");
  }
  return expanded;
}

// :!CMD -- the screen goes away, as for :make, and CMD has the terminal: an
// interactive program works too. vi waits in waitpid meanwhile.
static CommandResult editor_bang(Editor* editor, const char* command) {
  while (*command == ' ') {
    ++command;
  }
  if (*command == '\0') {
    editor_set_error_message(editor, "Usage: :!command");
    return CommandResult_Error;
  }
  char* expanded = editor_expand_command(editor, command);
  if (expanded == NULL) {
    return CommandResult_Error;
  }
  editor_record_flush(editor);
  endwin();
  printf("%s\n", expanded);
  fflush(stdout);
  const int status = run_shell_wait(expanded);
  if (status < 0) {
    printf("cannot run %s\n", MAKE_SHELL);
  }
  printf("\n[exit %d] press Enter", status);
  fflush(stdout);
  run_wait_for_enter();
  editor_resume_screen(editor);
  char message[96];
  snprintf(message, sizeof(message), "!%.60s: exit %d", expanded, status);
  free(expanded);
  editor_set_status_message(editor, message);
  return status == 0 ? CommandResult_Success : CommandResult_Error;
}

// The lines of `path` below the current line; the cursor goes to the first
// of them. Returns how many, or -1 if the file cannot be read.
static int editor_insert_file_below(Editor* editor, const char* path) {
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    return -1;
  }
  Buffer* buffer = editor->current_buffer;
  BufferRow* original = buffer->current_row;
  int count = 0;
  char* line = NULL;
  size_t capacity = 0;
  ssize_t length;
  while (original != NULL && (length = getline(&line, &capacity, file)) != -1) {
    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
      line[--length] = '\0';
    }
    BufferRow* above = buffer->current_row;
    buffer_break_current_line(buffer, (int)buffer_row_get_length(above));
    if (above->next == NULL || above->next == buffer->current_row) {
      break;  // out of memory: no row was added
    }
    buffer_row_replace_line(above->next, line);
    buffer->current_row = above->next;
    ++count;
  }
  free(line);
  fclose(file);
  buffer->current_row = original;
  if (count > 0) {
    editor_note_change(editor);
    editor_mark_dirty_whole_screen(editor);
    editor_move_cursor_y(editor, 1);
    buffer_scroll_rows(buffer, 1);
    editor_home_cursor_x(editor);
  }
  return count;
}

// :r !CMD (its stdout and stderr, stdin from /dev/null) and :r FILE.
static CommandResult editor_read(Editor* editor, const char* args) {
  if (editor->current_buffer == NULL || editor->current_buffer->current_row == NULL) {
    editor_set_error_message(editor, "No buffer");
    return CommandResult_Error;
  }
  char message[96];
  if (args[0] != '!') {
    if (args[0] == '\0') {
      editor_set_error_message(editor, "Usage: :r FILE or :r !command");
      return CommandResult_Error;
    }
    const int count = editor_insert_file_below(editor, args);
    if (count < 0) {
      editor_set_error_message(editor, "Cannot read the file");
      return CommandResult_Error;
    }
    snprintf(message, sizeof(message), "\"%.60s\" %d lines", args, count);
    editor_set_status_message(editor, message);
    return CommandResult_Success;
  }
  const char* command = args + 1;
  while (*command == ' ') {
    ++command;
  }
  if (*command == '\0') {
    editor_set_error_message(editor, "Usage: :r !command");
    return CommandResult_Error;
  }
  char* expanded = editor_expand_command(editor, command);
  if (expanded == NULL) {
    return CommandResult_Error;
  }
  editor_record_flush(editor);
  const int status = run_shell_to_file(expanded, RUN_OUTPUT_PATH);
  const int count = status < 0 ? -1 : editor_insert_file_below(editor, RUN_OUTPUT_PATH);
  remove(RUN_OUTPUT_PATH);
  if (status < 0 || count < 0) {
    snprintf(message, sizeof(message), "Cannot run %s", MAKE_SHELL);
  } else {
    snprintf(message, sizeof(message), "!%.50s: %d lines, exit %d", expanded, count,
             status);
  }
  free(expanded);
  editor_set_status_message(editor, message);
  return status == 0 ? CommandResult_Success : CommandResult_Error;
}

// :cn :cp :cc [N]
static CommandResult editor_step_error(Editor* editor, int step, const char* args) {
  MakeState* state = editor->make;
  if (state == NULL || state->errors.count == 0) {
    editor_set_status_message(editor, "No errors (run :make)");
    return CommandResult_Error;
  }
  int index = state->errors.current < 0 ? 0 : state->errors.current;
  if (step == 0 && args != NULL && args[0] >= '1' && args[0] <= '9') {
    index = atoi(args) - 1;
  } else if (state->errors.current >= 0 || step < 0) {
    index += step;
  }
  if (index >= state->errors.count) {
    editor_set_status_message(editor, "No more errors");
    return CommandResult_Error;
  }
  if (index < 0) {
    editor_set_status_message(editor, "Already at the first error");
    return CommandResult_Error;
  }
  return editor_goto_error(editor, index) ? CommandResult_Success
                                          : CommandResult_Error;
}

// The argument of `name` in the command line: "" for the bare command, the
// text after the spaces for "name ARGS", NULL when the line is another command.
static const char* editor_command_argument(const char* line, const char* name) {
  const size_t length = strlen(name);
  if (strncmp(line, name, length) != 0) {
    return NULL;
  }
  const char* rest = line + length;
  if (*rest == '\0') {
    return rest;
  }
  if (*rest != ' ') {
    return NULL;
  }
  while (*rest == ' ') {
    ++rest;
  }
  return rest;
}

// :man, :make, :set and the error list. False when the line is none of them.
static bool editor_process_build_command(Editor* editor, CommandResult* result) {
  const char* line = editor->command.buffer;
  const char* args;
  if ((args = editor_command_argument(line, "man")) != NULL) {
    *result =
      editor_show_man(editor, args) ? CommandResult_Success : CommandResult_Error;
    return true;
  }
  if ((args = editor_command_argument(line, "make")) != NULL) {
    *result = editor_make(editor, args);
    return true;
  }
  if (line[0] == '!') {
    *result = editor_bang(editor, line + 1);
    return true;
  }
  if ((args = editor_command_argument(line, "term")) != NULL ||
      (args = editor_command_argument(line, "terminal")) != NULL) {
    *result = editor_term(editor, args);
    return true;
  }
  if ((args = editor_command_argument(line, "termclose")) != NULL) {
    editor_terminal_close(editor);
    *result = CommandResult_Success;
    return true;
  }
  if ((args = editor_command_argument(line, "r")) != NULL ||
      (args = editor_command_argument(line, "read")) != NULL) {
    *result = editor_read(editor, args);
    return true;
  }
  if ((args = editor_command_argument(line, "set")) != NULL) {
    *result = editor_set_option(editor, args);
    return true;
  }
  if ((args = editor_command_argument(line, "cn")) != NULL ||
      (args = editor_command_argument(line, "cnext")) != NULL) {
    *result = editor_step_error(editor, 1, args);
    return true;
  }
  if ((args = editor_command_argument(line, "cp")) != NULL ||
      (args = editor_command_argument(line, "cprevious")) != NULL) {
    *result = editor_step_error(editor, -1, args);
    return true;
  }
  if ((args = editor_command_argument(line, "cc")) != NULL) {
    *result = editor_step_error(editor, 0, args);
    return true;
  }
  if ((args = editor_command_argument(line, "cl")) != NULL ||
      (args = editor_command_argument(line, "clist")) != NULL) {
    const char* log = editor->make != NULL && editor->make->log_path != NULL
                        ? editor->make->log_path
                        : MAKE_LOG_PATH;
    *result = editor_open_view(editor, log, true) ? CommandResult_Success
                                                  : CommandResult_Error;
    return true;
  }
  return false;
}

bool editor_restore_session(Editor* editor, const char* path) {
  MakeState* state = editor_make_state(editor);
  if (state == NULL) {
    return false;
  }
  MakeSession session = {NULL, 0, 0};
  if (!make_session_read(path, &session, &state->config)) {
    return false;
  }
  remove(path);
  const size_t first = editor->number_of_buffers;
  for (int i = 0; i < session.count; ++i) {
    const size_t before = editor->number_of_buffers;
    editor_load_file(editor, session.files[i].path);
    if (editor->number_of_buffers > before) {
      editor->buffers[before]->autosave_disabled =
        session.files[i].autosave_disabled;
    }
  }
  if (editor->number_of_buffers == first) {
    editor_create_new_file(editor);
  } else if (session.current < session.count &&
             first + (size_t)session.current < editor->number_of_buffers) {
    const MakeSessionFile* current = &session.files[session.current];
    editor_switch_to_buffer_by_index(editor, first + (size_t)session.current);
    int line = current->line;
    const int last = buffer_get_number_of_lines(editor->current_buffer) - 1;
    if (line > last) {
      line = last;
    }
    editor_goto_position(editor, line < 0 ? 0 : line, current->column);
  }
  make_session_clear(&session);
  const int status = make_read_status(MAKE_LOG_PATH);
  remove(MAKE_LOG_PATH MAKE_RC_SUFFIX);
  editor_make_report(editor, MAKE_LOG_PATH, status);
  return true;
}
