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

#include "toolbar_widget.h"

#include <stdlib.h>
#include <string.h>

Widget* widget_create(const char* name, WidgetPosition position, int priority,
                      WidgetVisibility visibility, const WidgetVTable* vtable,
                      void* data) {
  Widget* widget = (Widget*)malloc(sizeof(Widget));
  if (!widget) {
    return NULL;
  }

  widget->name = name;
  widget->position = position;
  widget->priority = priority;
  widget->visibility = visibility;
  widget->visible = (visibility != WidgetVisibility_OnDemand);
  widget->cached_width = 0;
  widget->cached_x = 0;
  widget->data = data;
  widget->vtable = vtable;
  widget->next = NULL;

  return widget;
}

void widget_destroy(Widget* widget) {
  if (!widget) {
    return;
  }

  if (widget->vtable && widget->vtable->deinit) {
    widget->vtable->deinit(widget);
  }

  free(widget);
}
