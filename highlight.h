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

typedef enum {
  EHighlightToken_Normal = 0,
  EHighlightToken_Keyword,
  EHighlightToken_String,
  EHighlightToken_Comment,
  EHighlightToken_Type,
  EHighlightToken_Preprocessor,
  EHighlightToken_Digit,
  EHighlightToken_Symbol,
  EHighlightToken_Keyword2,
  EHighlightToken_Symbol2,
  EHighlightToken_Count,
} EHighlightToken;