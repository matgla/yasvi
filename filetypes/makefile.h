/*
 Copyright (c) 2026 Mateusz Stadnik

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

#include "filetype.h"

// Makefiles: Makefile, makefile, GNUmakefile, Makefile.in/.am, *.mk, *.mak.
// Directives (include, ifeq, define, export...) as keywords, the variable an
// assignment sets as a type and its operator (= := ::= ?= += !=) as a
// symbol, rule targets (special ones such as .PHONY as keywords), `:` and
// `|`, `%` patterns, $(VAR) ${VAR} $@ $< references and $(function ...)
// calls, # comments. A line starting with a tab is a recipe: highlighted as
// shell with make's references on top. Backslash continuations and
// define ... endef bodies are carried to the next line. The filetype keeps
// hard tabs: Tab inserts a tab character, the indent step is a tab.
extern const Filetype g_filetype_make;

// Whether `text` is a rule line ("target: prerequisites"), the line a
// recipe follows -- not an assignment, a directive, a comment or a recipe.
bool makefile_is_rule(const char* text, int length);
