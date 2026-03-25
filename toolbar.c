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

#include "toolbar.h"

#include "editor.h"

#include <stdlib.h>
#include <string.h>

// Layout metrics for positioning
typedef struct {
  int total_width;
  int left_width;
  int right_width;
  int center_min_width;
} LayoutMetrics;

// Calculate total width needed for a widget list
static int calculate_list_width(Widget* list) {
  int total = 0;
  for (Widget* w = list; w != NULL; w = w->next) {
    if (w->visible && w->vtable && w->vtable->get_min_width) {
      total += w->vtable->get_min_width(w);
      // Add spacing between widgets
      if (w->next && w->next->visible) {
        total += 1;
      }
    }
  }
  return total;
}

// Sort widget into list by priority
static void insert_widget_sorted(Widget** list, Widget* widget) {
  if (*list == NULL || (*list)->priority > widget->priority) {
    widget->next = *list;
    *list = widget;
    return;
  }

  Widget* current = *list;
  while (current->next != NULL && current->next->priority <= widget->priority) {
    current = current->next;
  }
  widget->next = current->next;
  current->next = widget;
}

void toolbar_init(Toolbar* toolbar, int window_height, int window_width) {
  toolbar->height = 3;  // 2-line toolbar + 1 separator line
  toolbar->y_position = window_height - toolbar->height;
  toolbar->width = window_width;
  toolbar->left_widgets = NULL;
  toolbar->right_widgets = NULL;
  toolbar->center_widgets = NULL;
  toolbar->background_attr = COLOR_OTHER;
  toolbar->needs_redraw = true;
  toolbar->debug_mode = false;
}

void toolbar_deinit(Toolbar* toolbar) {
  if (!toolbar) {
    return;
  }

  // Free all widget lists
  Widget* lists[] = {toolbar->left_widgets, toolbar->right_widgets,
                     toolbar->center_widgets};
  for (int i = 0; i < 3; i++) {
    Widget* current = lists[i];
    while (current != NULL) {
      Widget* next = current->next;
      widget_destroy(current);
      current = next;
    }
  }

  toolbar->left_widgets = NULL;
  toolbar->right_widgets = NULL;
  toolbar->center_widgets = NULL;
}

bool toolbar_add_widget(Toolbar* toolbar, Widget* widget) {
  if (!toolbar || !widget) {
    return false;
  }

  switch (widget->position) {
    case WidgetPosition_Left:
      insert_widget_sorted(&toolbar->left_widgets, widget);
      break;
    case WidgetPosition_Right:
      insert_widget_sorted(&toolbar->right_widgets, widget);
      break;
    case WidgetPosition_Center:
    case WidgetPosition_Floating:
      insert_widget_sorted(&toolbar->center_widgets, widget);
      break;
  }

  // Initialize widget if it has an init function
  if (widget->vtable && widget->vtable->init) {
    if (!widget->vtable->init(widget, toolbar)) {
      toolbar_remove_widget(toolbar, widget->name);
      return false;
    }
  }

  toolbar->needs_redraw = true;
  return true;
}

void toolbar_remove_widget(Toolbar* toolbar, const char* name) {
  if (!toolbar || !name) {
    return;
  }

  Widget** lists[] = {&toolbar->left_widgets, &toolbar->right_widgets,
                      &toolbar->center_widgets};

  for (int i = 0; i < 3; i++) {
    Widget** current = lists[i];
    while (*current != NULL) {
      if (strcmp((*current)->name, name) == 0) {
        Widget* to_remove = *current;
        *current = (*current)->next;
        widget_destroy(to_remove);
        toolbar->needs_redraw = true;
        return;
      }
      current = &(*current)->next;
    }
  }
}

Widget* toolbar_find_widget(Toolbar* toolbar, const char* name) {
  if (!toolbar || !name) {
    return NULL;
  }

  Widget* lists[] = {toolbar->left_widgets, toolbar->right_widgets,
                     toolbar->center_widgets};

  for (int i = 0; i < 3; i++) {
    for (Widget* w = lists[i]; w != NULL; w = w->next) {
      if (strcmp(w->name, name) == 0) {
        return w;
      }
    }
  }

  return NULL;
}

void toolbar_layout(Toolbar* toolbar) {
  if (!toolbar) {
    return;
  }

  // Calculate widths
  (void)calculate_list_width(toolbar->left_widgets);
  (void)calculate_list_width(toolbar->right_widgets);
  int center_width = calculate_list_width(toolbar->center_widgets);

  // Position left widgets
  int x = 0;
  for (Widget* w = toolbar->left_widgets; w != NULL; w = w->next) {
    if (w->visible && w->vtable && w->vtable->get_min_width) {
      w->cached_x = x;
      w->cached_width = w->vtable->get_min_width(w);
      x += w->cached_width;
      if (w->next && w->next->visible) {
        x += 1;  // spacing
      }
    }
  }

  // Position right widgets (from right edge)
  x = toolbar->width;
  // Count visible right widgets and process in reverse
  Widget* right_list[16];  // Max 16 right widgets
  int right_count = 0;
  for (Widget* w = toolbar->right_widgets; w != NULL && right_count < 16;
       w = w->next) {
    if (w->visible) {
      right_list[right_count++] = w;
    }
  }
  // Position from right to left
  for (int i = right_count - 1; i >= 0; i--) {
    Widget* w = right_list[i];
    if (w->vtable && w->vtable->get_min_width) {
      int width = w->vtable->get_min_width(w);
      x -= width;
      w->cached_x = x;
      w->cached_width = width;
      if (i > 0) {
        x -= 1;  // spacing
      }
    }
  }

  // Center widgets
  if (center_width > 0 && toolbar->center_widgets) {
    int center_x = (toolbar->width - center_width) / 2;
    x = center_x;
    for (Widget* w = toolbar->center_widgets; w != NULL; w = w->next) {
      if (w->visible && w->vtable && w->vtable->get_min_width) {
        w->cached_x = x;
        w->cached_width = w->vtable->get_min_width(w);
        x += w->cached_width;
        if (w->next && w->next->visible) {
          x += 1;
        }
      }
    }
  }
}

void toolbar_draw(Toolbar* toolbar, void* editor) {
  (void)editor;
  if (!toolbar) {
    return;
  }

  // Clear toolbar area
  attrset(toolbar->background_attr);
  for (int row = 0; row < toolbar->height; row++) {
    move(toolbar->y_position + row, 0);
    for (int i = 0; i < toolbar->width; i++) {
      addch(' ');
    }
  }

  // Draw separator line (horizontal line with dashes or just leave blank)
  // Line 0 is separator, lines 1-2 are actual toolbar content
  attrset(COLOR_OTHER | A_DIM);
  move(toolbar->y_position, 0);
  for (int i = 0; i < toolbar->width; i++) {
    addch('-');
  }
  attrset(COLOR_OTHER);

  // Draw all visible widgets on row 1 (main toolbar row)
  // Separator is row 0, main toolbar is row 1, command line is row 2
  Widget* lists[] = {toolbar->left_widgets, toolbar->right_widgets,
                     toolbar->center_widgets};

  for (int i = 0; i < 3; i++) {
    for (Widget* w = lists[i]; w != NULL; w = w->next) {
      if (w->visible && w->cached_width > 0 && w->vtable && w->vtable->draw) {
        int max_width = toolbar->width - w->cached_x;
        if (max_width > 0) {
          int row = toolbar->y_position + 1;
          // Command and message widgets go on the bottom row
          if (strcmp(w->name, "command") == 0 || strcmp(w->name, "message") == 0) {
            row = toolbar->y_position + 2;
          }
          w->vtable->draw(w, toolbar, w->cached_x, row, max_width);
        }
      }
    }
  }

  toolbar->needs_redraw = false;
}

void toolbar_invalidate(Toolbar* toolbar) {
  if (toolbar) {
    toolbar->needs_redraw = true;
  }
}

void toolbar_resize(Toolbar* toolbar, int window_height, int window_width) {
  if (!toolbar) {
    return;
  }

  toolbar->y_position = window_height - toolbar->height;
  toolbar->width = window_width;
  toolbar_layout(toolbar);
  toolbar->needs_redraw = true;
}

void toolbar_update(Toolbar* toolbar, void* editor) {
  if (!toolbar) {
    return;
  }

  Widget* lists[] = {toolbar->left_widgets, toolbar->right_widgets,
                     toolbar->center_widgets};

  bool needs_layout = false;
  for (int i = 0; i < 3; i++) {
    for (Widget* w = lists[i]; w != NULL; w = w->next) {
      if (w->vtable && w->vtable->update) {
        WidgetUpdateResult result = w->vtable->update(w, toolbar, (void*)editor);
        if (result == WidgetUpdate_Redraw) {
          toolbar->needs_redraw = true;
          needs_layout = true;
        }
      }
    }
  }

  // Recalculate layout if any widget changed (content size might have changed)
  if (needs_layout) {
    toolbar_layout(toolbar);
  }
}
