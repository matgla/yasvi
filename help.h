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

#include <stddef.h>

#include "buffer.h"

// The name the help buffer gets, so ':help' can find and reuse it.
#define HELP_BUFFER_NAME "yasvi-help"

// malloc'd Buffer named HELP_BUFFER_NAME, filled with the built-in help.
Buffer* help_alloc_buffer(void);
