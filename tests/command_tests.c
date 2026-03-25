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

#include "command.h"

#include <string.h>

void test_command_init(void) {
  Command command;
  command_init(&command);

  TEST_CHECK(command.buffer != NULL);
  TEST_CHECK(command.buffer[0] == '\0');
  TEST_CHECK(command.buffer_size == 64);  // COMMAND_BUFFER_DEFAULT_SIZE
  TEST_CHECK(command.cursor_position == 0);

  command_deinit(&command);
}

void test_command_append_single(void) {
  Command command;
  command_init(&command);

  command_append(&command, 'h');
  TEST_CHECK(strcmp(command.buffer, "h") == 0);
  TEST_CHECK(command.cursor_position == 1);

  command_append(&command, 'i');
  TEST_CHECK(strcmp(command.buffer, "hi") == 0);
  TEST_CHECK(command.cursor_position == 2);

  command_deinit(&command);
}

void test_command_append_multiple(void) {
  Command command;
  command_init(&command);

  const char* test_string = "hello";
  for (size_t i = 0; i < strlen(test_string); i++) {
    command_append(&command, test_string[i]);
  }

  TEST_CHECK(strcmp(command.buffer, "hello") == 0);
  TEST_CHECK(command.cursor_position == 5);

  command_deinit(&command);
}

void test_command_append_reallocation(void) {
  Command command;
  command_init(&command);

  // Initial size is 64, fill up to 62 characters
  // The condition for reallocation is: cursor_position == buffer_size - 1
  // At position 63, reallocation will happen (doubling to 128)
  for (int i = 0; i < 62; i++) {
    command_append(&command, 'a');
  }

  TEST_CHECK(command.cursor_position == 62);
  TEST_CHECK(command.buffer_size == 64);

  // Add character at position 62, cursor becomes 63
  command_append(&command, 'b');
  TEST_CHECK(command.cursor_position == 63);
  TEST_CHECK(command.buffer_size == 64);  // Still 64, haven't hit position 63 == 63 check yet

  // This append will trigger reallocation:
  // cursor_position (63) == buffer_size (64) - 1, so reallocate to 128
  command_append(&command, 'c');
  TEST_CHECK(command.cursor_position == 64);
  TEST_CHECK(command.buffer_size == 128);

  // Verify all previous data is intact
  TEST_CHECK(command.buffer[0] == 'a');
  TEST_CHECK(command.buffer[61] == 'a');
  TEST_CHECK(command.buffer[62] == 'b');
  TEST_CHECK(command.buffer[63] == 'c');
  TEST_CHECK(command.buffer[64] == '\0');

  command_deinit(&command);
}

void test_command_append_many_chars(void) {
  Command command;
  command_init(&command);

  // Append many characters to thoroughly test reallocation
  for (int i = 0; i < 200; i++) {
    command_append(&command, 'x');
  }

  TEST_CHECK(command.cursor_position == 200);
  TEST_CHECK(command.buffer_size >= 256);
  TEST_CHECK(strlen(command.buffer) == 200);

  // Verify content
  for (int i = 0; i < 200; i++) {
    TEST_CHECK(command.buffer[i] == 'x');
  }
  TEST_CHECK(command.buffer[200] == '\0');

  command_deinit(&command);
}

void test_command_deinit(void) {
  Command command;
  command_init(&command);

  command_append(&command, 't');
  command_append(&command, 'e');
  command_append(&command, 's');
  command_append(&command, 't');

  command_deinit(&command);

  TEST_CHECK(command.buffer == NULL);
  TEST_CHECK(command.buffer_size == 0);
  TEST_CHECK(command.cursor_position == 0);
}

void test_command_error_simple(void) {
  Command command;
  command_init(&command);

  // Simulate some command input
  command_append(&command, 'w');
  command_append(&command, 'q');
  TEST_CHECK(strcmp(command.buffer, "wq") == 0);
  TEST_CHECK(command.cursor_position == 2);

  // Set an error message
  command_error(&command, "Error: No file name");

  // The error message should be at the start of the buffer
  // Use strncmp to check only the message part (not garbage bytes after)
  const char* message = "Error: No file name";
  size_t msg_len = strlen(message);
  TEST_CHECK(strncmp(command.buffer, message, msg_len) == 0);
  // Null terminator should be at position: cursor_position + error_length
  TEST_CHECK(command.buffer[command.cursor_position + msg_len] == '\0');
  // Cursor position should remain unchanged
  TEST_CHECK(command.cursor_position == 2);

  command_deinit(&command);
}

void test_command_error_with_content(void) {
  Command command;
  command_init(&command);

  // Add some content
  const char* input = ":write";
  for (size_t i = 0; i < strlen(input); i++) {
    command_append(&command, input[i]);
  }
  TEST_CHECK(command.cursor_position == 6);

  // Set error message
  command_error(&command, "Unknown command");

  // The error message should be at the start
  const char* message = "Unknown command";
  size_t msg_len = strlen(message);
  TEST_CHECK(strncmp(command.buffer, message, msg_len) == 0);
  // Null terminator at correct position
  TEST_CHECK(command.buffer[command.cursor_position + msg_len] == '\0');
  // Cursor position remains at 6
  TEST_CHECK(command.cursor_position == 6);

  command_deinit(&command);
}

void test_command_error_short_message(void) {
  Command command;
  command_init(&command);

  // Add longer content
  const char* input = "save file";
  for (size_t i = 0; i < strlen(input); i++) {
    command_append(&command, input[i]);
  }
  TEST_CHECK(command.cursor_position == 9);

  // Set a short error message (shorter than cursor_position)
  // In this case, some of the original content survives
  command_error(&command, "Err");

  // Error message at start
  TEST_CHECK(strncmp(command.buffer, "Err", 3) == 0);
  // Original content shifted from positions 0-8 to positions 3-11
  // But positions 3-5 were overwritten by 'r' characters from the message
  // Let's verify null terminator is at correct position
  TEST_CHECK(command.buffer[3 + 9] == '\0');

  command_deinit(&command);
}

void test_command_error_reallocation(void) {
  Command command;
  command_init(&command);

  // Fill buffer to near capacity
  for (int i = 0; i < 60; i++) {
    command_append(&command, 'x');
  }

  size_t old_size = command.buffer_size;

  // Error message that would exceed buffer capacity
  const char* long_error = "This is a very long error message that will definitely exceed the buffer size and trigger reallocation";
  command_error(&command, long_error);

  // Buffer should have been reallocated
  TEST_CHECK(command.buffer_size > old_size);

  // Check content is correct - error message should be at the start
  size_t error_len = strlen(long_error);
  TEST_CHECK(strncmp(command.buffer, long_error, error_len) == 0);

  command_deinit(&command);
}

void test_command_append_after_error(void) {
  Command command;
  command_init(&command);

  // Add content and set error
  command_append(&command, 'w');
  command_append(&command, 'q');
  command_error(&command, "Error");

  // Buffer should have "Error" at start (wq was overwritten since message is longer)
  TEST_CHECK(strncmp(command.buffer, "Error", 5) == 0);
  // Cursor position remains at 2
  TEST_CHECK(command.cursor_position == 2);

  // Continue appending - the '!' goes at position 2, replacing 'r'
  command_append(&command, '!');
  // Now cursor should be at position 3 (2 + 1)
  TEST_CHECK(command.cursor_position == 3);
  // Buffer is "Er!" (error message with '!' inserted at cursor position)
  TEST_CHECK(strncmp(command.buffer, "Er!", 3) == 0);
  // Null terminator should be at position 3
  TEST_CHECK(command.buffer[3] == '\0');

  command_deinit(&command);
}

void test_command_sequence_write_quit(void) {
  Command command;
  command_init(&command);

  // Simulate typing :wq
  command_append(&command, 'w');
  command_append(&command, 'q');

  TEST_CHECK(strcmp(command.buffer, "wq") == 0);
  TEST_CHECK(command.cursor_position == 2);

  command_deinit(&command);
}

void test_command_sequence_write_file(void) {
  Command command;
  command_init(&command);

  // Simulate typing :w test.txt
  const char* input = "w test.txt";
  for (size_t i = 0; i < strlen(input); i++) {
    command_append(&command, input[i]);
  }

  TEST_CHECK(strcmp(command.buffer, "w test.txt") == 0);
  TEST_CHECK(command.cursor_position == 10);

  command_deinit(&command);
}

void test_command_empty_buffer_operations(void) {
  Command command;
  command_init(&command);

  // Buffer starts empty
  TEST_CHECK(command.buffer[0] == '\0');

  // Add one char
  command_append(&command, 'x');
  TEST_CHECK(strcmp(command.buffer, "x") == 0);

  // Deinit and check reset
  command_deinit(&command);
  TEST_CHECK(command.buffer == NULL);
  TEST_CHECK(command.buffer_size == 0);
  TEST_CHECK(command.cursor_position == 0);
}

TEST_LIST = {
    {"test_command_init", test_command_init},
    {"test_command_append_single", test_command_append_single},
    {"test_command_append_multiple", test_command_append_multiple},
    {"test_command_append_reallocation", test_command_append_reallocation},
    {"test_command_append_many_chars", test_command_append_many_chars},
    {"test_command_deinit", test_command_deinit},
    {"test_command_error_simple", test_command_error_simple},
    {"test_command_error_with_content", test_command_error_with_content},
    {"test_command_error_short_message", test_command_error_short_message},
    {"test_command_error_reallocation", test_command_error_reallocation},
    {"test_command_append_after_error", test_command_append_after_error},
    {"test_command_sequence_write_quit", test_command_sequence_write_quit},
    {"test_command_sequence_write_file", test_command_sequence_write_file},
    {"test_command_empty_buffer_operations", test_command_empty_buffer_operations},

    {NULL, NULL}  // zeroed record marking the end of the list
};
