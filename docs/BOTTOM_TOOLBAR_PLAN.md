# Widget-Based Bottom Toolbar Architecture Plan

## Overview

This document outlines the architecture for a modular, widget-based bottom toolbar system to replace the current hardcoded status bar implementation in yasvi.

## Current State

The current status bar (`editor_draw_status_bar()`) uses hardcoded positions:
- **Line 1 (height - 2)**: Static status bar text (`editor->status_bar`)
- **Line 2 (height - 1)**: Command input, error messages, key sequence, and debug key info

```c
// Current hardcoded approach in editor.c
mvaddch(editor->window.height - 1, 0, ':');              // Command prompt
mvaddstr(editor->window.height - 1, 1, editor->command.buffer);
mvaddstr(editor->window.height - 1, 1, editor->error_message);  // Error message
mvaddstr(editor->window.height - 2, 0, editor->status_bar);     // Status bar
mvaddstr(editor->window.height - 1, editor->window.width - 10, editor->key_sequence);
mvprintw(editor->window.height - 1, editor->window.width - 30, "'%c'(%d) ", editor->key, editor->key);
```

## Proposed Architecture

### 1. Widget Type System

```c
// toolbar_widget.h
#pragma once

#include <stdbool.h>
#include <stddef.h>

// Widget visibility modes

typedef enum {
  WidgetVisibility_Always,      // Always visible (e.g., mode indicator)
  WidgetVisibility_OnDemand,    // Show when active (e.g., command input)
  WidgetVisibility_Debug,       // Show only in debug builds
  WidgetVisibility_Configurable // User-toggleable
} WidgetVisibility;

// Widget position modes

typedef enum {
  WidgetPosition_Left,      // Left-aligned, fixed order
  WidgetPosition_Right,     // Right-aligned, fixed order  
  WidgetPosition_Center,    // Center-aligned
  WidgetPosition_Floating   // Position specified in columns
} WidgetPosition;

// Widget update result

typedef enum {
  WidgetUpdate_Redraw,      // Widget needs redraw
  WidgetUpdate_NoChange,    // No visual change
  WidgetUpdate_Hidden       // Widget should be hidden
} WidgetUpdateResult;

// Forward declaration
struct Widget;
struct Toolbar;

// Widget virtual table - function pointers for polymorphic behavior

typedef struct {
  // Initialize widget state, returns false on failure
  bool (*init)(struct Widget* widget, struct Toolbar* toolbar);
  
  // Clean up widget resources
  void (*deinit)(struct Widget* widget);
  
  // Update widget content, returns whether redraw is needed
  WidgetUpdateResult (*update)(struct Widget* widget, struct Toolbar* toolbar);
  
  // Draw widget at specified position, returns actual width drawn
  int (*draw)(const struct Widget* widget, struct Toolbar* toolbar, int x, int y);
  
  // Get minimum width needed for widget content
  int (*get_min_width)(const struct Widget* widget);
  
  // Get preferred width (0 = use min width)
  int (*get_preferred_width)(const struct Widget* widget);
  
  // Handle key press (optional, for interactive widgets)
  bool (*handle_key)(struct Widget* widget, int key);
} WidgetVTable;

// Base widget structure

typedef struct Widget {
  const char* name;              // Widget identifier
  WidgetVisibility visibility;
  WidgetPosition position;
  int priority;                  // Drawing order (lower = first)
  bool visible;
  int cached_width;              // Last drawn width
  int cached_x;                  // Last drawn position
  
  // Widget-specific data (subclassing via composition)
  void* data;
  
  // Virtual table
  const WidgetVTable* vtable;
  
  // Linked list for toolbar
  struct Widget* next;
} Widget;

// Widget factory function type

typedef Widget* (*WidgetFactory)(void);
```

### 2. Toolbar Container

```c
// toolbar.h
#pragma once

#include "toolbar_widget.h"
#include <ncurses.h>

typedef struct Toolbar {
  int y_position;           // Screen row where toolbar starts
  int height;              // Number of rows (1 or 2)
  int width;               // Screen width
  
  // Widget lists by position
  Widget* left_widgets;
  Widget* right_widgets;
  Widget* center_widgets;
  
  // Background/clearing character
  chtype background_char;
  attr_t background_attr;
  
  // Dirty flag for full redraw
  bool needs_redraw;
  
  // Debug mode flag
  bool debug_mode;
} Toolbar;

// Toolbar lifecycle
void toolbar_init(Toolbar* toolbar, int window_height, int window_width);
void toolbar_deinit(Toolbar* toolbar);

// Widget management
bool toolbar_add_widget(Toolbar* toolbar, Widget* widget);
void toolbar_remove_widget(Toolbar* toolbar, const char* name);
Widget* toolbar_find_widget(Toolbar* toolbar, const char* name);

// Layout and rendering
void toolbar_layout(Toolbar* toolbar);
void toolbar_draw(Toolbar* toolbar);
void toolbar_invalidate(Toolbar* toolbar);  // Mark for full redraw

// Resize handling
void toolbar_resize(Toolbar* toolbar, int window_height, int window_width);

// Update all widgets (call before draw)
void toolbar_update(Toolbar* toolbar, void* context);  // context = Editor*
```

### 3. Layout Engine

```
┌─────────────────────────────────────────────────────────────────┐
│  [MODE]  [FILENAME]                    [ROW:COL]  [ENCODING]   │  <- Left | Right split
├─────────────────────────────────────────────────────────────────┤
│  :command input here...                      [keyseq] [debug]  │  <- Command line + debug
└─────────────────────────────────────────────────────────────────┘
   ^                                            ^
   Left-aligned widgets              Right-aligned widgets
```

**Layout Algorithm:**

1. **Measure Phase**: Query all visible widgets for minimum/preferred widths
2. **Left Pass**: Position left-aligned widgets from left edge, left-to-right
3. **Right Pass**: Position right-aligned widgets from right edge, right-to-left
4. **Center Pass**: Position center widgets between left and right groups
5. **Conflict Resolution**: If left + right overlap, truncate right widgets or hide lower priority

```c
// layout.c - Internal layout implementation

typedef struct {
  int total_width;
  int left_width;
  int right_width;
  int center_width;
  int available_center;
} LayoutMetrics;

static void layout_calculate_metrics(Toolbar* toolbar, LayoutMetrics* metrics) {
  // Sum widths for each position group
  // Handle overflow by hiding lowest-priority widgets
}

static int layout_get_left_edge(const Widget* widget, const LayoutMetrics* metrics) {
  // Calculate x position based on widget position and previous widgets
}
```

### 4. Built-in Widgets

#### 4.1 Mode Widget (vim-like mode indicator)
```c
Widget* widget_mode_create(void);
// Shows: -- NORMAL --, -- INSERT --, -- COMMAND --
```

#### 4.2 Filename Widget
```c
Widget* widget_filename_create(void);
// Shows: current buffer filename, modified indicator [*]
```

#### 4.3 Position Widget
```c
Widget* widget_position_create(void);
// Shows: Line:Column / TotalLines
```

#### 4.4 Command Input Widget
```c
Widget* widget_command_create(void);
// Shows: ':' prompt + command buffer
// Handles: Interactive input, cursor positioning
```

#### 4.5 Error/Message Widget
```c
Widget* widget_message_create(void);
// Shows: Error messages, status messages
// Auto-hides after timeout or on key press
```

#### 4.6 Key Sequence Widget
```c
Widget* widget_keyseq_create(void);
// Shows: Partial key sequences (e.g., "d" waiting for next key)
```

#### 4.7 Debug Keystroke Widget (NEW)
```c
// widget_debug_keystroke.h
#pragma once

#include "toolbar_widget.h"

// Configuration for the debug widget

typedef struct {
  bool show_ascii_char;      // Show printable character representation
  bool show_decimal_code;    // Show decimal key code
  bool show_hex_code;        // Show hexadecimal key code
  bool show_octal_code;      // Show octal key code
  int history_size;          // Number of keystrokes to remember (0 = disable)
  bool compact_mode;         // Single char display vs full info
} DebugKeystrokeConfig;

Widget* widget_debug_keystroke_create(const DebugKeystrokeConfig* config);

// Update with new keystroke
void widget_debug_keystroke_add_key(Widget* widget, int key);

// Clear history
void widget_debug_keystroke_clear(Widget* widget);
```

**Debug Widget Display Modes:**

```
Compact mode:    [k:127|DEL]
Full mode:       ['\x7f' 127 0x7f 0177 DEL]  
History mode:    [ESC k a]  (last 3 keys)

Examples:
  Regular 'a':   ['a' 97 0x61 0141]
  Enter:         ['\n' 10 0x0a 0012 LF]
  Escape:        ['\x1b' 27 0x1b 0033 ESC]
  Backspace:     ['\x7f' 127 0x7f 0177 DEL]
  Arrow Up:      ['?' 259 0x103 0403 KEY_UP]
```

**Implementation:**

```c
// widget_debug_keystroke.c

typedef struct {
  DebugKeystrokeConfig config;
  int last_key;
  int* history;
  int history_count;
  int history_pos;
} DebugKeystrokeData;

static int debug_keystroke_draw(const Widget* widget, Toolbar* toolbar, int x, int y) {
  DebugKeystrokeData* data = (DebugKeystrokeData*)widget->data;
  if (data->last_key == 0) return 0;
  
  char buffer[64];
  int len = 0;
  
  if (data->config.compact_mode) {
    // [k:127|DEL]
    const char* name = get_key_name(data->last_key);
    len = snprintf(buffer, sizeof(buffer), "[k:%d|%s]", data->last_key, name);
  } else {
    // ['\x7f' 127 0x7f 0177 DEL]
    char ascii_repr[8];
    get_ascii_representation(data->last_key, ascii_repr, sizeof(ascii_repr));
    const char* name = get_key_name(data->last_key);
    
    len = snprintf(buffer, sizeof(buffer), "[%s %d 0x%x %04o %s]",
                   ascii_repr, data->last_key, data->last_key, 
                   data->last_key, name);
  }
  
  mvaddnstr(y, x, buffer, toolbar->width - x - 1);
  return len;
}

static const char* get_key_name(int key) {
  switch (key) {
    case 27: return "ESC";
    case '\n':
    case '\r': return "LF";
    case 127: return "DEL";
    case 9: return "TAB";
    case ' ': return "SPC";
    case KEY_UP: return "UP";
    case KEY_DOWN: return "DOWN";
    case KEY_LEFT: return "LEFT";
    case KEY_RIGHT: return "RIGHT";
    case KEY_BACKSPACE: return "BS";
    case KEY_DC: return "DC";
    case KEY_HOME: return "HOME";
    case KEY_END: return "END";
    case KEY_PPAGE: return "PGUP";
    case KEY_NPAGE: return "PGDN";
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
```

### 5. Integration with Editor

```c
// Updated editor.h

typedef struct {
  // ... existing fields ...
  
  // Replace status_bar with toolbar
  Toolbar* toolbar;
  
  // Keep for backwards compatibility during transition
  char* status_bar;  // TODO: Remove after migration
} Editor;

// editor.c - Initialization

void editor_init(Editor* editor) {
  window_init(&editor->window);
  
  // Create toolbar
  editor->toolbar = malloc(sizeof(Toolbar));
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
  
  editor_home_cursor_xy(editor);
  move(editor->cursor.y, editor->cursor.x);
}

// editor.c - Key processing

void editor_process_key(Editor* editor, int key) {
  // Update debug widget with keystroke
  Widget* debug_widget = toolbar_find_widget(editor->toolbar, "debug_keystroke");
  if (debug_widget) {
    widget_debug_keystroke_add_key(debug_widget, key);
  }
  
  // ... rest of key processing ...
}

// editor.c - Drawing

void editor_redraw_screen(Editor* editor) {
  curs_set(0);
  editor_draw_buffers(editor);
  
  // Update and draw toolbar
  toolbar_update(editor->toolbar, editor);
  toolbar_draw(editor->toolbar);
  
  switch (editor->state) {
    case EditorState_Running:
    case EditorState_EditMode:
      editor_restore_cursor_position(editor);
      break;
    default:
      break;
  }
  window_redraw_screen(&editor->window);
  curs_set(1);
}
```

### 6. Configuration System

```c
// toolbar_config.h

// Runtime widget configuration via config file or commands

typedef struct {
  const char* widget_name;
  WidgetPosition position;
  int priority;
  WidgetVisibility visibility;
} WidgetConfigEntry;

// Load widget configuration from file
void toolbar_load_config(Toolbar* toolbar, const char* config_path);

// Save current widget configuration
void toolbar_save_config(const Toolbar* toolbar, const char* config_path);

// Toggle widget visibility at runtime
void toolbar_toggle_widget(Toolbar* toolbar, const char* name);

// Example config file format:
/*
# yasvi toolbar configuration

widget mode {
  position = left
  priority = 1
  visibility = always
}

widget filename {
  position = left
  priority = 2
  visibility = always
}

widget position {
  position = right
  priority = 1
  visibility = always
}

widget debug_keystroke {
  position = right
  priority = 10
  visibility = debug  # Only in debug builds
  config {
    compact_mode = true
    show_hex = true
  }
}
*/
```

## Migration Plan

### Phase 1: Core Infrastructure
1. Create `toolbar_widget.h/c` - Base widget system
2. Create `toolbar.h/c` - Toolbar container
3. Create simple test widgets (mode, position)
4. Add toggle to switch between old/new toolbar (compile-time flag)

### Phase 2: Feature Parity
1. Implement all existing status bar features as widgets:
   - Command input widget
   - Error message widget
   - Key sequence widget
   - Filename widget
2. Remove old hardcoded `editor_draw_status_bar()`
3. Make new toolbar the default

### Phase 3: Debug Widget
1. Implement `widget_debug_keystroke.c`
2. Add to toolbar in debug builds
3. Document usage for UART debugging

### Phase 4: Polish
1. Configuration file support
2. Runtime widget toggling
3. Custom user widgets (plugin API)

## File Structure

```
yasvi/
├── toolbar/
│   ├── toolbar.h
│   ├── toolbar.c
│   ├── toolbar_widget.h
│   ├── toolbar_layout.c
│   ├── toolbar_config.c
│   └── widgets/
│       ├── widget_mode.c
│       ├── widget_filename.c
│       ├── widget_position.c
│       ├── widget_command.c
│       ├── widget_message.c
│       ├── widget_keyseq.c
│       └── widget_debug_keystroke.c
├── docs/
│   └── BOTTOM_TOOLBAR_PLAN.md (this file)
└── tests/
    └── toolbar/
        └── toolbar_tests.c
```

## Use Case: UART Keycode Debugging

The debug keystroke widget addresses the specific need for debugging UART input issues:

**Scenario**: Target machine sends wrong keycodes due to incomplete UART driver.

**Setup**:
```c
// In editor_init(), only in debug builds
#ifdef DEBUG
DebugKeystrokeConfig config = {
  .compact_mode = false,    // Show full details
  .show_hex_code = true,    // See raw hex values
  .show_decimal_code = true,
  .history_size = 5,        // Remember last 5 keys
};
toolbar_add_widget(toolbar, widget_debug_keystroke_create(&config));
#endif
```

**Visual Output**:
```
┌────────────────────────────────────────────────────────────┐
│  test.c [+]                          15:23  0x7f 127 DEL   │
├────────────────────────────────────────────────────────────┤
│  :w                                                         │
└────────────────────────────────────────────────────────────┘
```

**What the user sees when pressing Backspace**:
- Expected: `['\x7f' 127 0x7f 0177 DEL]` (on most terminals)
- Wrong mapping: If UART sends `0x08` instead, user sees `['\b' 8 0x08 0010 BS]`
- This helps identify the mismatch between expected and actual keycodes

## Open Questions

1. **Widget Communication**: Should widgets subscribe to editor events, or should editor push updates?
2. **Theming**: How to integrate with existing color system?
3. **Multi-line Toolbar**: Support for 2+ line toolbars (vim-style with airline)?
4. **Widget Ordering**: Should users be able to reorder widgets at runtime?
