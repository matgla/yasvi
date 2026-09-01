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

#include "acutest.h"

#include "mock_ncurses.h"

#include "editor.h"
#include "toolbar.h"
#include "toolbar_widgets.h"

#include <string.h>

/* The toolbar has its own row for the command line, so widgets living there
   must not be pushed right by the widgets of the main row above them. */

static void toolbar_setup(Toolbar* toolbar) {
  toolbar_init(toolbar, 24, 80);
  toolbar_add_widget(toolbar, widget_mode_create());
  toolbar_add_widget(toolbar, widget_filename_create());
  toolbar_add_widget(toolbar, widget_position_create());
  toolbar_add_widget(toolbar, widget_command_create());
  toolbar_add_widget(toolbar, widget_message_create());
  toolbar_add_widget(toolbar, widget_keyseq_create());
}

/* Enough of an editor for the widgets to read themselves out of. */
static void editor_setup(Editor* editor, Buffer* buffer) {
  memset(editor, 0, sizeof(*editor));
  editor->state = EditorState_Running;
  editor->window.width = 80;
  editor->window.height = 24;
  editor->number_of_line_digits = 4;
  editor->cursor.x = 4;
  editor->cursor.y = 2;
  editor->current_buffer = buffer;
}

static Buffer* buffer_with_lines(int count) {
  Buffer* buffer = buffer_alloc();
  for (int i = 0; i < count; ++i) {
    buffer_append_line(buffer, "some text on a line");
  }
  buffer_scroll_to_top(buffer);
  return buffer;
}

void test_toolbar_command_widget_is_on_its_own_row(void) {
  Toolbar toolbar;
  toolbar_setup(&toolbar);

  Widget* command = toolbar_find_widget(&toolbar, "command");
  Widget* message = toolbar_find_widget(&toolbar, "message");
  Widget* mode = toolbar_find_widget(&toolbar, "mode");

  TEST_ASSERT(command != NULL && message != NULL && mode != NULL);
  TEST_CHECK(command->row == 1);
  TEST_CHECK(message->row == 1);
  TEST_CHECK(mode->row == 0);

  toolbar_deinit(&toolbar);
}

void test_toolbar_command_starts_at_begin_of_line(void) {
  Toolbar toolbar;
  toolbar_setup(&toolbar);

  Widget* command = toolbar_find_widget(&toolbar, "command");
  TEST_ASSERT(command != NULL);
  command->visible = true;

  toolbar_layout(&toolbar);

  /* Not pushed right by "-- NORMAL --" and the filename on the row above */
  TEST_CHECK(command->cached_x == 0);

  toolbar_deinit(&toolbar);
}

void test_toolbar_main_row_still_flows_left_to_right(void) {
  Toolbar toolbar;
  toolbar_setup(&toolbar);

  Widget* command = toolbar_find_widget(&toolbar, "command");
  Widget* mode = toolbar_find_widget(&toolbar, "mode");
  Widget* filename = toolbar_find_widget(&toolbar, "filename");

  TEST_ASSERT(command != NULL && mode != NULL && filename != NULL);
  command->visible = true;

  toolbar_layout(&toolbar);

  /* The main row keeps its own flow: mode first, filename after it */
  TEST_CHECK(mode->cached_x == 0);
  TEST_CHECK(filename->cached_x == mode->cached_width + 1);

  toolbar_deinit(&toolbar);
}

void test_toolbar_command_row_is_below_main_row(void) {
  Toolbar toolbar;
  toolbar_setup(&toolbar);

  Widget* command = toolbar_find_widget(&toolbar, "command");
  Widget* mode = toolbar_find_widget(&toolbar, "mode");
  TEST_ASSERT(command != NULL && mode != NULL);

  /* Row 0 is the separator line, so content starts one line below it */
  TEST_CHECK(toolbar.y_position + 1 + mode->row == toolbar.y_position + 1);
  TEST_CHECK(toolbar.y_position + 1 + command->row == toolbar.y_position + 2);
  TEST_CHECK(toolbar.height == 3);

  toolbar_deinit(&toolbar);
}

/* The widths of the widgets on a row decide where the ones after them start,
   so a widget sized to its own content drags its neighbours sideways whenever
   that content changes. These pin the fields that change while editing. */

void test_toolbar_filename_holds_still_when_the_mode_changes(void) {
  Toolbar toolbar;
  toolbar_setup(&toolbar);
  Buffer* buffer = buffer_with_lines(3);
  Editor editor;
  editor_setup(&editor, buffer);

  Widget* filename = toolbar_find_widget(&toolbar, "filename");
  TEST_ASSERT(filename != NULL);

  toolbar_update(&toolbar, &editor);
  const int x_in_normal_mode = filename->cached_x;

  /* "-- COMMAND --" is a character longer than "-- NORMAL --" */
  editor.state = EditorState_CollectingCommand;
  toolbar_update(&toolbar, &editor);
  TEST_CHECK(filename->cached_x == x_in_normal_mode);

  editor.state = EditorState_EditMode;
  toolbar_update(&toolbar, &editor);
  TEST_CHECK(filename->cached_x == x_in_normal_mode);

  buffer_free(buffer);
  toolbar_deinit(&toolbar);
}

void test_toolbar_position_holds_still_as_the_cursor_moves(void) {
  Toolbar toolbar;
  toolbar_setup(&toolbar);
  Buffer* buffer = buffer_with_lines(120);
  Editor editor;
  editor_setup(&editor, buffer);

  Widget* position = toolbar_find_widget(&toolbar, "position");
  TEST_ASSERT(position != NULL);

  toolbar_update(&toolbar, &editor);
  const int x_on_first_line = position->cached_x;
  const int width_on_first_line = position->cached_width;

  /* Line 1 -> 100: three digits more, and the readout is right aligned, so a
     content-sized field would slide its whole text across the bar. */
  buffer_scroll_rows(buffer, 99);
  editor.cursor.x = 12;
  toolbar_update(&toolbar, &editor);

  TEST_CHECK(position->cached_x == x_on_first_line);
  TEST_CHECK(position->cached_width == width_on_first_line);

  buffer_free(buffer);
  toolbar_deinit(&toolbar);
}

void test_toolbar_position_holds_still_when_a_key_sequence_appears(void) {
  Toolbar toolbar;
  toolbar_setup(&toolbar);
  Buffer* buffer = buffer_with_lines(10);
  Editor editor;
  editor_setup(&editor, buffer);

  Widget* position = toolbar_find_widget(&toolbar, "position");
  TEST_ASSERT(position != NULL);

  toolbar_update(&toolbar, &editor);
  const int x_without_sequence = position->cached_x;

  /* Half a multi-key command: its readout shares the right edge with the
     position, whose slot must not be pushed aside to make room. */
  strcpy(editor.key_sequence, "d");
  toolbar_update(&toolbar, &editor);
  TEST_CHECK(position->cached_x == x_without_sequence);

  editor.key_sequence[0] = '\0';
  toolbar_update(&toolbar, &editor);
  TEST_CHECK(position->cached_x == x_without_sequence);

  buffer_free(buffer);
  toolbar_deinit(&toolbar);
}

TEST_LIST = {
    {"test_toolbar_command_widget_is_on_its_own_row",
     test_toolbar_command_widget_is_on_its_own_row},
    {"test_toolbar_command_starts_at_begin_of_line",
     test_toolbar_command_starts_at_begin_of_line},
    {"test_toolbar_main_row_still_flows_left_to_right",
     test_toolbar_main_row_still_flows_left_to_right},
    {"test_toolbar_command_row_is_below_main_row",
     test_toolbar_command_row_is_below_main_row},
    {"test_toolbar_filename_holds_still_when_the_mode_changes",
     test_toolbar_filename_holds_still_when_the_mode_changes},
    {"test_toolbar_position_holds_still_as_the_cursor_moves",
     test_toolbar_position_holds_still_as_the_cursor_moves},
    {"test_toolbar_position_holds_still_when_a_key_sequence_appears",
     test_toolbar_position_holds_still_when_a_key_sequence_appears},

    {NULL, NULL} /* zeroed record marking the end of the list */
};
