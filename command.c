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

#include "command.h"

#include <string.h>

#define COMMAND_BUFFER_DEFAULT_SIZE 64

void command_init(Command* command) {
  command->buffer = malloc(COMMAND_BUFFER_DEFAULT_SIZE);
  command->buffer[0] = '\0';
  command->buffer_size = COMMAND_BUFFER_DEFAULT_SIZE;
  command->cursor_position = 0;
}

void command_deinit(Command* command) {
  free(command->buffer);
  command->buffer = NULL;
  command->buffer_size = 0;
  command->cursor_position = 0;
}

void command_append(Command* command, char ch) {
  if (command->cursor_position == command->buffer_size - 1) {
    command->buffer_size <<= 1;
    command->buffer = realloc(command->buffer, command->buffer_size);
  }

  command->buffer[command->cursor_position++] = ch;
  command->buffer[command->cursor_position] = '\0';
}

void command_error(Command* command, const char* message) {
  int error_length = strlen(message);
  if (error_length + command->cursor_position >= command->buffer_size) {
    command->buffer_size = error_length + command->cursor_position + 1;
    command->buffer = realloc(command->buffer, command->buffer_size);
  }
  memcpy(command->buffer + command->cursor_position, command->buffer,
         command->cursor_position);
  memcpy(command->buffer, message, error_length);
  command->buffer[command->cursor_position + error_length] = '\0';
}