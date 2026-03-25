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

#include "toolbar_widgets.h"

#include "buffer.h"
#include "editor.h"
#include "toolbar.h"

#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Helper Functions
// ============================================================================

static const char* get_key_name(int key) {
  switch (key) {
    case 27:
      return "ESC";
    case '\n':
    case '\r':
      return "LF";
    case 127:
      return "DEL";
    case 9:
      return "TAB";
    case ' ':
      return "SPC";
    case KEY_UP:
      return "UP";
    case KEY_DOWN:
      return "DOWN";
    case KEY_LEFT:
      return "LEFT";
    case KEY_RIGHT:
      return "RIGHT";
    case KEY_BACKSPACE:
      return "BS";
    case KEY_DC:
      return "DC";
    case KEY_HOME:
      return "HOME";
    case KEY_END:
      return "END";
    case KEY_PPAGE:
      return "PGUP";
    case KEY_NPAGE:
      return "PGDN";
    case KEY_IC:
      return "INS";
    default:
      if (key >= 0 && key < 32) return "CTL";
      if (key >= ' ' && key <= '~') {
        static char buf[2] = {0};
        buf[0] = (char)key;
        return buf;
      }
      return "???";
  }
}

static void get_ascii_representation(int key, char* out, size_t out_size) {
  if (key >= ' ' && key <= '~') {
    snprintf(out, out_size, "'%c'", (char)key);
  } else if (key == '\n') {
    snprintf(out, out_size, "'\\n'");
  } else if (key == '\r') {
    snprintf(out, out_size, "'\\r'");
  } else if (key == '\t') {
    snprintf(out, out_size, "'\\t'");
  } else if (key == 27) {
    snprintf(out, out_size, "'\\x1b'");
  } else if (key == 127) {
    snprintf(out, out_size, "'\\x7f'");
  } else if (key >= 0 && key < 32) {
    snprintf(out, out_size, "'\\x%02x'", key);
  } else {
    snprintf(out, out_size, "'?'");
  }
}

// ============================================================================
// Mode Widget
// ============================================================================

typedef struct {
  char mode_text[16];
  EditorState last_state;
} ModeWidgetData;

static bool mode_init(Widget* widget, struct Toolbar* toolbar) {
  (void)toolbar;
  ModeWidgetData* data = (ModeWidgetData*)widget->data;
  strcpy(data->mode_text, "");
  data->last_state = -1;
  return true;
}

static void mode_deinit(Widget* widget) {
  free(widget->data);
}

static WidgetUpdateResult mode_update(Widget* widget, struct Toolbar* toolbar,
                                      void* editor_ctx) {
  Editor* editor = (Editor*)editor_ctx;
  (void)toolbar;
  ModeWidgetData* data = (ModeWidgetData*)widget->data;

  if (editor->state == data->last_state) {
    return WidgetUpdate_NoChange;
  }
  data->last_state = editor->state;

  switch (editor->state) {
    case EditorState_Running:
      strcpy(data->mode_text, "-- NORMAL --");
      break;
    case EditorState_EditMode:
      strcpy(data->mode_text, "-- INSERT --");
      break;
    case EditorState_CollectingCommand:
    case EditorState_ProcessingCommand:
      strcpy(data->mode_text, "-- COMMAND --");
      break;
    case EditorState_Exiting:
      strcpy(data->mode_text, "-- EXITING --");
      break;
    default:
      strcpy(data->mode_text, "");
  }

  return WidgetUpdate_Redraw;
}

static int mode_draw(const Widget* widget, struct Toolbar* toolbar, int x, int y,
                     int max_width) {
  (void)toolbar;
  const ModeWidgetData* data = (const ModeWidgetData*)widget->data;
  int len = strlen(data->mode_text);
  if (len > max_width) len = max_width;

  attrset(COLOR_OTHER | A_BOLD);
  mvaddnstr(y, x, data->mode_text, len);
  attrset(COLOR_OTHER);
  return len;
}

static int mode_get_min_width(const Widget* widget) {
  const ModeWidgetData* data = (const ModeWidgetData*)widget->data;
  int len = strlen(data->mode_text);
  return len > 0 ? len : 12;  // 12 = strlen("-- NORMAL --")
}

static const WidgetVTable mode_vtable = {
    .init = mode_init,
    .deinit = mode_deinit,
    .update = mode_update,
    .draw = mode_draw,
    .get_min_width = mode_get_min_width,
    .handle_key = NULL,
};

Widget* widget_mode_create(void) {
  ModeWidgetData* data = malloc(sizeof(ModeWidgetData));
  if (!data) return NULL;

  return widget_create("mode", WidgetPosition_Left, 1, WidgetVisibility_Always,
                       &mode_vtable, data);
}

// ============================================================================
// Filename Widget
// ============================================================================

typedef struct {
  char filename[256];
  bool modified;
} FilenameWidgetData;

static bool filename_init(Widget* widget, struct Toolbar* toolbar) {
  (void)toolbar;
  FilenameWidgetData* data = (FilenameWidgetData*)widget->data;
  data->filename[0] = '\0';
  data->modified = false;
  return true;
}

static void filename_deinit(Widget* widget) {
  free(widget->data);
}

static WidgetUpdateResult filename_update(Widget* widget, struct Toolbar* toolbar,
                                          void* editor_ctx) {
  Editor* editor = (Editor*)editor_ctx;
  (void)toolbar;
  FilenameWidgetData* data = (FilenameWidgetData*)widget->data;

  const char* fname = buffer_get_filename(editor->current_buffer);
  if (!fname) fname = "[No Name]";

  if (strcmp(data->filename, fname) == 0 && data->modified == buffer_is_modified(editor->current_buffer)) {
    return WidgetUpdate_NoChange;
  }

  strncpy(data->filename, fname, sizeof(data->filename) - 1);
  data->filename[sizeof(data->filename) - 1] = '\0';
  data->modified = buffer_is_modified(editor->current_buffer);

  return WidgetUpdate_Redraw;
}

static int filename_draw(const Widget* widget, struct Toolbar* toolbar, int x,
                         int y, int max_width) {
  (void)toolbar;
  const FilenameWidgetData* data = (const FilenameWidgetData*)widget->data;

  char display[300];
  int len = snprintf(display, sizeof(display), "%s%s", data->filename,
                     data->modified ? " [+]" : "");
  if (len > max_width) len = max_width;

  attrset(COLOR_OTHER);
  mvaddnstr(y, x, display, len);
  return len;
}

static int filename_get_min_width(const Widget* widget) {
  const FilenameWidgetData* data = (const FilenameWidgetData*)widget->data;
  int len = strlen(data->filename);
  if (data->modified) len += 4;
  return len > 0 ? len : 10;
}

static const WidgetVTable filename_vtable = {
    .init = filename_init,
    .deinit = filename_deinit,
    .update = filename_update,
    .draw = filename_draw,
    .get_min_width = filename_get_min_width,
    .handle_key = NULL,
};

Widget* widget_filename_create(void) {
  FilenameWidgetData* data = malloc(sizeof(FilenameWidgetData));
  if (!data) return NULL;

  return widget_create("filename", WidgetPosition_Left, 2, WidgetVisibility_Always,
                       &filename_vtable, data);
}

// ============================================================================
// Position Widget
// ============================================================================

typedef struct {
  int line;
  int column;
  int total_lines;
  bool valid;
} PositionWidgetData;

static bool position_init(Widget* widget, struct Toolbar* toolbar) {
  (void)toolbar;
  PositionWidgetData* data = (PositionWidgetData*)widget->data;
  data->line = 0;
  data->column = 0;
  data->total_lines = 0;
  data->valid = false;
  return true;
}

static void position_deinit(Widget* widget) {
  free(widget->data);
}

static WidgetUpdateResult position_update(Widget* widget, struct Toolbar* toolbar,
                                          void* editor_ctx) {
  Editor* editor = (Editor*)editor_ctx;
  (void)toolbar;
  PositionWidgetData* data = (PositionWidgetData*)widget->data;

  int current_line = buffer_get_current_line_number(editor->current_buffer);
  int current_col = editor_get_cursor_x(editor) + 1;
  int total = buffer_get_number_of_lines(editor->current_buffer);

  if (data->line == current_line && data->column == current_col &&
      data->total_lines == total && data->valid) {
    return WidgetUpdate_NoChange;
  }

  data->line = current_line;
  data->column = current_col;
  data->total_lines = total;
  data->valid = true;

  return WidgetUpdate_Redraw;
}

static int position_draw(const Widget* widget, struct Toolbar* toolbar, int x,
                         int y, int max_width) {
  (void)toolbar;
  const PositionWidgetData* data = (const PositionWidgetData*)widget->data;

  char display[64];
  int len =
      snprintf(display, sizeof(display), "%d:%d/%d", data->line, data->column, data->total_lines);
  if (len > max_width) len = max_width;

  attrset(COLOR_OTHER);
  mvaddnstr(y, x, display, len);
  return len;
}

static int position_get_min_width(const Widget* widget) {
  const PositionWidgetData* data = (const PositionWidgetData*)widget->data;
  if (!data->valid) return 12;  // "9999:99/9999"
  // Return actual width based on content
  char display[64];
  int len = snprintf(display, sizeof(display), "%d:%d/%d", 
                     data->line, data->column, data->total_lines);
  return len > 0 ? len : 12;
}

static const WidgetVTable position_vtable = {
    .init = position_init,
    .deinit = position_deinit,
    .update = position_update,
    .draw = position_draw,
    .get_min_width = position_get_min_width,
    .handle_key = NULL,
};

Widget* widget_position_create(void) {
  PositionWidgetData* data = malloc(sizeof(PositionWidgetData));
  if (!data) return NULL;

  return widget_create("position", WidgetPosition_Right, 1, WidgetVisibility_Always,
                       &position_vtable, data);
}

// ============================================================================
// Command Widget
// ============================================================================

typedef struct {
  char command[256];
  bool active;
} CommandWidgetData;

static bool widget_command_init(Widget* widget, struct Toolbar* toolbar) {
  (void)toolbar;
  CommandWidgetData* data = (CommandWidgetData*)widget->data;
  data->command[0] = '\0';
  data->active = false;
  return true;
}

static void widget_command_deinit(Widget* widget) {
  free(widget->data);
}

static WidgetUpdateResult widget_command_update(Widget* widget, struct Toolbar* toolbar,
                                         void* editor_ctx) {
  Editor* editor = (Editor*)editor_ctx;
  (void)toolbar;
  CommandWidgetData* data = (CommandWidgetData*)widget->data;

  bool was_active = data->active;
  data->active = (editor->state == EditorState_CollectingCommand ||
                  editor->state == EditorState_ProcessingCommand);

  if (!data->active) {
    if (was_active) {
      data->command[0] = '\0';
      widget->visible = false;
      return WidgetUpdate_Redraw;
    }
    return WidgetUpdate_NoChange;
  }

  widget->visible = true;

  const char* cmd = editor->command.buffer;
  if (!cmd) cmd = "";

  if (strcmp(data->command, cmd) == 0 && was_active) {
    return WidgetUpdate_NoChange;
  }

  strncpy(data->command, cmd, sizeof(data->command) - 1);
  data->command[sizeof(data->command) - 1] = '\0';

  return WidgetUpdate_Redraw;
}

static int widget_command_draw(const Widget* widget, struct Toolbar* toolbar, int x, int y,
                        int max_width) {
  (void)toolbar;
  const CommandWidgetData* data = (const CommandWidgetData*)widget->data;

  attrset(COLOR_OTHER | A_BOLD);
  mvaddch(y, x, ':');
  attrset(COLOR_OTHER);

  int len = strlen(data->command);
  if (len > max_width - 1) len = max_width - 1;
  if (len > 0) {
    mvaddnstr(y, x + 1, data->command, len);
  }
  return 1 + len;
}

static int widget_command_get_min_width(const Widget* widget) {
  const CommandWidgetData* data = (const CommandWidgetData*)widget->data;
  return 1 + strlen(data->command);  // ':' + command
}

static const WidgetVTable command_vtable = {
    .init = widget_command_init,
    .deinit = widget_command_deinit,
    .update = widget_command_update,
    .draw = widget_command_draw,
    .get_min_width = widget_command_get_min_width,
    .handle_key = NULL,
};

Widget* widget_command_create(void) {
  CommandWidgetData* data = malloc(sizeof(CommandWidgetData));
  if (!data) return NULL;

  Widget* w = widget_create("command", WidgetPosition_Left, 10,
                            WidgetVisibility_OnDemand, &command_vtable, data);
  w->visible = false;  // Hidden by default
  return w;
}

// ============================================================================
// Message Widget (for errors and status)
// ============================================================================

typedef struct {
  char message[512];
  bool has_message;
} MessageWidgetData;

static bool message_init(Widget* widget, struct Toolbar* toolbar) {
  (void)toolbar;
  MessageWidgetData* data = (MessageWidgetData*)widget->data;
  data->message[0] = '\0';
  data->has_message = false;
  return true;
}

static void message_deinit(Widget* widget) {
  free(widget->data);
}

static WidgetUpdateResult message_update(Widget* widget, struct Toolbar* toolbar,
                                         void* editor_ctx) {
  Editor* editor = (Editor*)editor_ctx;
  (void)toolbar;
  MessageWidgetData* data = (MessageWidgetData*)widget->data;

  if (editor->error_message) {
    if (strcmp(data->message, editor->error_message) == 0 && data->has_message) {
      return WidgetUpdate_NoChange;
    }
    strncpy(data->message, editor->error_message, sizeof(data->message) - 1);
    data->message[sizeof(data->message) - 1] = '\0';
    data->has_message = true;
    widget->visible = true;
    return WidgetUpdate_Redraw;
  }

  if (data->has_message) {
    data->message[0] = '\0';
    data->has_message = false;
    widget->visible = false;
    return WidgetUpdate_Redraw;
  }

  return WidgetUpdate_NoChange;
}

static int message_draw(const Widget* widget, struct Toolbar* toolbar, int x, int y,
                        int max_width) {
  (void)toolbar;
  const MessageWidgetData* data = (const MessageWidgetData*)widget->data;

  int len = strlen(data->message);
  if (len > max_width) len = max_width;

  attrset(COLOR_ERROR);
  mvaddnstr(y, x, data->message, len);
  attrset(COLOR_OTHER);
  return len;
}

static int message_get_min_width(const Widget* widget) {
  const MessageWidgetData* data = (const MessageWidgetData*)widget->data;
  return data->has_message ? strlen(data->message) : 0;
}

static const WidgetVTable message_vtable = {
    .init = message_init,
    .deinit = message_deinit,
    .update = message_update,
    .draw = message_draw,
    .get_min_width = message_get_min_width,
    .handle_key = NULL,
};

Widget* widget_message_create(void) {
  MessageWidgetData* data = malloc(sizeof(MessageWidgetData));
  if (!data) return NULL;

  Widget* w = widget_create("message", WidgetPosition_Center, 1,
                            WidgetVisibility_OnDemand, &message_vtable, data);
  w->visible = false;  // Hidden by default
  return w;
}

// ============================================================================
// Key Sequence Widget
// ============================================================================

typedef struct {
  char sequence[32];
} KeyseqWidgetData;

static bool keyseq_init(Widget* widget, struct Toolbar* toolbar) {
  (void)toolbar;
  KeyseqWidgetData* data = (KeyseqWidgetData*)widget->data;
  data->sequence[0] = '\0';
  return true;
}

static void keyseq_deinit(Widget* widget) {
  free(widget->data);
}

static WidgetUpdateResult keyseq_update(Widget* widget, struct Toolbar* toolbar,
                                        void* editor_ctx) {
  Editor* editor = (Editor*)editor_ctx;
  (void)toolbar;
  KeyseqWidgetData* data = (KeyseqWidgetData*)widget->data;

  if (editor->key_sequence[0] == '\0') {
    if (data->sequence[0] == '\0') {
      return WidgetUpdate_NoChange;
    }
    data->sequence[0] = '\0';
    widget->visible = false;
    return WidgetUpdate_Redraw;
  }

  if (strcmp(data->sequence, editor->key_sequence) == 0) {
    return WidgetUpdate_NoChange;
  }

  strncpy(data->sequence, editor->key_sequence, sizeof(data->sequence) - 1);
  data->sequence[sizeof(data->sequence) - 1] = '\0';
  widget->visible = true;
  return WidgetUpdate_Redraw;
}

static int keyseq_draw(const Widget* widget, struct Toolbar* toolbar, int x, int y,
                       int max_width) {
  (void)toolbar;
  const KeyseqWidgetData* data = (const KeyseqWidgetData*)widget->data;

  int len = strlen(data->sequence);
  if (len > max_width) len = max_width;

  attrset(COLOR_OTHER | A_DIM);
  mvaddnstr(y, x, data->sequence, len);
  attrset(COLOR_OTHER);
  return len;
}

static int keyseq_get_min_width(const Widget* widget) {
  const KeyseqWidgetData* data = (const KeyseqWidgetData*)widget->data;
  int len = strlen(data->sequence);
  return len > 0 ? len : 5;  // Minimum space for potential sequence
}

static const WidgetVTable keyseq_vtable = {
    .init = keyseq_init,
    .deinit = keyseq_deinit,
    .update = keyseq_update,
    .draw = keyseq_draw,
    .get_min_width = keyseq_get_min_width,
    .handle_key = NULL,
};

Widget* widget_keyseq_create(void) {
  KeyseqWidgetData* data = malloc(sizeof(KeyseqWidgetData));
  if (!data) return NULL;

  Widget* w = widget_create("keyseq", WidgetPosition_Right, 5, WidgetVisibility_Always,
                            &keyseq_vtable, data);
  w->visible = false;  // Hidden when empty
  return w;
}

// ============================================================================
// Debug Keystroke Widget
// ============================================================================

typedef struct {
  DebugKeystrokeConfig config;
  int last_key;
} DebugKeystrokeData;

static bool debug_keystroke_init(Widget* widget, struct Toolbar* toolbar) {
  (void)toolbar;
  DebugKeystrokeData* data = (DebugKeystrokeData*)widget->data;
  data->last_key = 0;
  return true;
}

static void debug_keystroke_deinit(Widget* widget) {
  free(widget->data);
}

static WidgetUpdateResult debug_keystroke_update(Widget* widget,
                                                 struct Toolbar* toolbar,
                                                 void* editor_ctx) {
  (void)editor_ctx;
  (void)toolbar;
  (void)editor_ctx;
  DebugKeystrokeData* data = (DebugKeystrokeData*)widget->data;

  if (data->last_key == 0) {
    return WidgetUpdate_NoChange;
  }

  // Reset after reading - the key is added via widget_debug_keystroke_add_key
  data->last_key = 0;
  return WidgetUpdate_Redraw;
}

static int debug_keystroke_draw(const Widget* widget, struct Toolbar* toolbar, int x,
                                int y, int max_width) {
  (void)toolbar;
  const DebugKeystrokeData* data = (const DebugKeystrokeData*)widget->data;

  if (data->last_key == 0) {
    return 0;
  }

  char buffer[128];
  int len = 0;

  if (data->config.compact_mode) {
    // Compact: [k:127|DEL]
    const char* name = get_key_name(data->last_key);
    len = snprintf(buffer, sizeof(buffer), "[k:%d|%s]", data->last_key, name);
  } else {
    // Full: ['\x7f' 127 0x7f 0177 DEL]
    char ascii_repr[16];
    get_ascii_representation(data->last_key, ascii_repr, sizeof(ascii_repr));
    const char* name = get_key_name(data->last_key);

    len = 0;
    len += snprintf(buffer + len, sizeof(buffer) - len, "[");

    if (data->config.show_ascii_char) {
      len += snprintf(buffer + len, sizeof(buffer) - len, "%s ", ascii_repr);
    }
    if (data->config.show_decimal_code) {
      len += snprintf(buffer + len, sizeof(buffer) - len, "%d ", data->last_key);
    }
    if (data->config.show_hex_code) {
      len += snprintf(buffer + len, sizeof(buffer) - len, "0x%x ", data->last_key);
    }
    if (data->config.show_octal_code) {
      len += snprintf(buffer + len, sizeof(buffer) - len, "%04o ", data->last_key);
    }

    len += snprintf(buffer + len, sizeof(buffer) - len, "%s]", name);
  }

  if (len > max_width) len = max_width;

  attrset(COLOR_OTHER);
  mvaddnstr(y, x, buffer, len);
  return len;
}

static int debug_keystroke_get_min_width(const Widget* widget) {
  const DebugKeystrokeData* data = (const DebugKeystrokeData*)widget->data;
  if (data->last_key == 0) return 12;  // Reserve space for first key
  if (data->config.compact_mode) return 12;
  return 30;
}

static const WidgetVTable debug_keystroke_vtable = {
    .init = debug_keystroke_init,
    .deinit = debug_keystroke_deinit,
    .update = debug_keystroke_update,
    .draw = debug_keystroke_draw,
    .get_min_width = debug_keystroke_get_min_width,
    .handle_key = NULL,
};

Widget* widget_debug_keystroke_create(const DebugKeystrokeConfig* config) {
  DebugKeystrokeData* data = malloc(sizeof(DebugKeystrokeData));
  if (!data) return NULL;

  if (config) {
    data->config = *config;
  } else {
    // Default config
    data->config.show_ascii_char = true;
    data->config.show_decimal_code = true;
    data->config.show_hex_code = true;
    data->config.show_octal_code = false;
    data->config.compact_mode = true;
  }
  data->last_key = 0;

  return widget_create("debug_keystroke", WidgetPosition_Right, 10,
                       WidgetVisibility_Debug, &debug_keystroke_vtable, data);
}

void widget_debug_keystroke_add_key(Widget* widget, int key) {
  if (!widget || !widget->data) return;

  DebugKeystrokeData* data = (DebugKeystrokeData*)widget->data;
  data->last_key = key;
}

void widget_debug_keystroke_clear(Widget* widget) {
  if (!widget || !widget->data) return;

  DebugKeystrokeData* data = (DebugKeystrokeData*)widget->data;
  data->last_key = 0;
}
