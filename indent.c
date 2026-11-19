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

#include "indent.h"

#include <ctype.h>
#include <string.h>

#include "filetypes/makefile.h"

// How far up indent_find_opener and indent_unit look.
#define INDENT_SEARCH_ROWS 2000
#define INDENT_UNIT_ROWS 500

IndentLanguage indent_language(const char* filetype_name) {
  if (filetype_name == NULL) {
    return IndentLanguage_None;
  }
  if (strcmp(filetype_name, "c") == 0 || strcmp(filetype_name, "zig") == 0 ||
      strcmp(filetype_name, "pio") == 0) {
    return IndentLanguage_Brackets;
  }
  if (strcmp(filetype_name, "shell") == 0) {
    return IndentLanguage_Shell;
  }
  if (strcmp(filetype_name, "make") == 0) {
    return IndentLanguage_Make;
  }
  return IndentLanguage_None;
}

bool indent_has_brackets(IndentLanguage language) {
  return language == IndentLanguage_Brackets || language == IndentLanguage_Shell;
}

int indent_leading(const char* text, int length) {
  int i = 0;
  while (i < length && (text[i] == ' ' || text[i] == '\t')) {
    ++i;
  }
  return i;
}

static bool indent_is_opener(char c) {
  return c == '{' || c == '(' || c == '[';
}

static bool indent_is_closer(char c) {
  return c == '}' || c == ')' || c == ']';
}

// Walks `text`, calling `visit` for every bracket outside strings, character
// literals and line comments. Returns the length of the code part (where a
// line comment starts, else `length`).
typedef void (*IndentVisit)(void* context, int position, char bracket);

static int indent_scan(IndentLanguage language,
                       const char* text,
                       int length,
                       IndentVisit visit,
                       void* context) {
  for (int i = 0; i < length; ++i) {
    const char c = text[i];
    if (language == IndentLanguage_Shell) {
      if (c == '#' && (i == 0 || isspace((unsigned char)text[i - 1]) || text[i - 1] == ';')) {
        return i;
      }
      if (c == '\\') {
        ++i;  // an escaped character
        continue;
      }
      if (c == '\'') {
        while (i + 1 < length && text[i + 1] != '\'') {
          ++i;
        }
        ++i;
        continue;
      }
    } else {
      if (c == '/' && i + 1 < length && text[i + 1] == '/') {
        return i;
      }
      if (c == '\\' && i + 1 < length && text[i + 1] == '\\') {
        return i;  // a Zig multiline string line
      }
      if (c == '\'') {
        while (i + 1 < length && text[i + 1] != '\'') {
          i += text[i + 1] == '\\' ? 2 : 1;
        }
        ++i;
        continue;
      }
    }
    if (c == '"') {
      while (i + 1 < length && text[i + 1] != '"') {
        i += text[i + 1] == '\\' ? 2 : 1;
      }
      ++i;
      continue;
    }
    if (visit != NULL && (indent_is_opener(c) || indent_is_closer(c))) {
      visit(context, i, c);
    }
  }
  return length;
}

static bool indent_word_ends_with(const char* text, int length, const char* word) {
  const int word_length = (int)strlen(word);
  if (length < word_length || strncmp(text + length - word_length, word, word_length) != 0) {
    return false;
  }
  if (length == word_length) {
    return true;
  }
  const char before = text[length - word_length - 1];
  return before == ' ' || before == '\t' || before == ';' || before == ')';
}

bool indent_opens_block(IndentLanguage language, const char* text, int length) {
  if (language == IndentLanguage_None) {
    return false;
  }
  if (language == IndentLanguage_Make) {
    return makefile_is_rule(text, length);
  }
  int end = indent_scan(language, text, length, NULL, NULL);
  while (end > 0 && isspace((unsigned char)text[end - 1])) {
    --end;
  }
  if (end == 0) {
    return false;
  }
  if (indent_is_opener(text[end - 1])) {
    return true;
  }
  if (language == IndentLanguage_Shell) {
    static const char* const words[] = {"then", "do", "else", "in"};
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
      if (indent_word_ends_with(text, end, words[i])) {
        // "in" opens a case; "for x in a b" ends in a word, not "in"
        return i != 3 || strncmp(text + indent_leading(text, end), "case", 4) == 0;
      }
    }
  }
  return false;
}

bool indent_starts_with_closer(const char* text, int length) {
  const int first = indent_leading(text, length);
  return first < length && indent_is_closer(text[first]);
}

// The brackets of one line in order.
typedef struct {
  int positions[256];
  char brackets[256];
  int count;
} IndentBrackets;

static void indent_collect(void* context, int position, char bracket) {
  IndentBrackets* list = (IndentBrackets*)context;
  if (list->count < (int)(sizeof(list->positions) / sizeof(list->positions[0]))) {
    list->positions[list->count] = position;
    list->brackets[list->count] = bracket;
    ++list->count;
  }
}

const BufferRow* indent_find_opener(IndentLanguage language,
                                    const BufferRow* row,
                                    int column) {
  int depth = 0;
  IndentBrackets list;
  for (int searched = 0; row != NULL && searched < INDENT_SEARCH_ROWS;
       ++searched, row = row->prev) {
    list.count = 0;
    indent_scan(language, row->data, row->len, indent_collect, &list);
    for (int i = list.count - 1; i >= 0; --i) {
      if (searched == 0 && list.positions[i] >= column) {
        continue;  // the closer itself and what follows it
      }
      if (indent_is_closer(list.brackets[i])) {
        ++depth;
      } else if (depth == 0) {
        return row;
      } else {
        --depth;
      }
    }
  }
  return NULL;
}

bool indent_detect(const BufferRow* first, bool* tabs_out, int* step_out) {
  // How often each step from 1 to 8 spaces appears between a line and the
  // next one indented deeper.
  int steps[9] = {0};
  int tabs = 0;
  int previous = -1;
  int searched = 0;
  for (const BufferRow* row = first; row != NULL && searched < INDENT_UNIT_ROWS;
       row = row->next, ++searched) {
    const int leading = indent_leading(row->data, row->len);
    if (leading == row->len) {
      continue;  // blank lines say nothing
    }
    if (row->data[0] == '\t') {
      ++tabs;
      previous = -1;
      continue;
    }
    if (previous >= 0 && leading > previous && leading - previous <= 8) {
      ++steps[leading - previous];
    }
    previous = leading;
  }
  int best = 0;
  for (int step = 1; step <= 8; ++step) {
    if (steps[step] > steps[best]) {
      best = step;
    }
  }
  if (tabs > steps[best]) {
    *tabs_out = true;
    return true;
  }
  if (best > 0) {
    *tabs_out = false;
    *step_out = best;
    return true;
  }
  return false;
}

int indent_unit(const BufferRow* first, int tab_size, char* out, int size) {
  bool tabs = false;
  int spaces = tab_size > 0 ? tab_size : 4;
  indent_detect(first, &tabs, &spaces);
  int length = 0;
  if (tabs) {
    if (size > 1) {
      out[length++] = '\t';
    }
  } else {
    while (length < spaces && length < size - 1) {
      out[length++] = ' ';
    }
  }
  if (size > 0) {
    out[length] = '\0';
  }
  return length;
}
