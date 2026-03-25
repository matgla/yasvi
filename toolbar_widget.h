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

// Forward declarations
struct Widget;
struct Toolbar;

// Editor is typedef'd in editor.h, forward declare as struct here
// Users of this header must include editor.h before including this header

// Widget visibility modes
typedef enum {
  WidgetVisibility_Always,       // Always visible
  WidgetVisibility_OnDemand,     // Show when active
  WidgetVisibility_Debug,        // Show only in debug builds
  WidgetVisibility_Configurable  // User-toggleable
} WidgetVisibility;

// Widget position modes
typedef enum {
  WidgetPosition_Left,     // Left-aligned
  WidgetPosition_Right,    // Right-aligned
  WidgetPosition_Center,   // Center-aligned
  WidgetPosition_Floating  // Position specified
} WidgetPosition;

// Widget update result
typedef enum {
  WidgetUpdate_Redraw,    // Widget needs redraw
  WidgetUpdate_NoChange,  // No visual change
  WidgetUpdate_Hidden     // Widget should be hidden
} WidgetUpdateResult;

// Widget virtual table - polymorphic behavior
typedef struct {
  // Initialize widget state
  bool (*init)(struct Widget* widget, struct Toolbar* toolbar);

  // Clean up widget resources
  void (*deinit)(struct Widget* widget);

  // Update widget content
  // Note: Editor must be defined (by including editor.h) before use
  WidgetUpdateResult (*update)(struct Widget* widget, struct Toolbar* toolbar,
                               void* editor);

  // Draw widget at specified position, returns actual width drawn
  int (*draw)(const struct Widget* widget, struct Toolbar* toolbar, int x, int y,
              int max_width);

  // Get minimum width needed
  int (*get_min_width)(const struct Widget* widget);

  // Handle key press (optional)
  bool (*handle_key)(struct Widget* widget, int key);
} WidgetVTable;

// Base widget structure
typedef struct Widget {
  const char* name;           // Widget identifier
  WidgetVisibility visibility;
  WidgetPosition position;
  int priority;               // Drawing order (lower = first)
  bool visible;
  int cached_width;           // Last drawn width
  int cached_x;               // Last drawn position

  void* data;                 // Widget-specific data
  const WidgetVTable* vtable; // Virtual table

  struct Widget* next;        // Linked list
} Widget;

// Widget creation helper
Widget* widget_create(const char* name, WidgetPosition position, int priority,
                      WidgetVisibility visibility, const WidgetVTable* vtable,
                      void* data);

// Widget destruction
void widget_destroy(Widget* widget);
