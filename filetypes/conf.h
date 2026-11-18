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

#include "filetype.h"

// Configuration files: yasvi's own (vi.conf, .yasvirc) and the usual
// key=value / [section] files (.conf .cfg .ini .toml, git's, fstab,
// inittab). Whole-line # and ; comments, [section] headers, a leading
// `set`, the key (yasvi's settings as keywords, any other key as a type,
// so a misspelt setting stands out), = : ? and the value: numbers and
// on/off/yes/no/true/false/wait/exec (and a setting's "no") as literals,
// quoted strings, $VAR and %.
extern const Filetype g_filetype_conf;
